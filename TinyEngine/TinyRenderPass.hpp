#ifndef TINY_ENGINE_RENDERPASS
#define TINY_ENGINE_RENDERPASS
	#include "./TinyEngine.hpp"

	namespace TINY_ENGINE_NAMESPACE {
        class TinyRenderObject {
        public:
			TinyPipeline& executionPipeline;
			std::pair<VkCommandBuffer, int32_t>& executionBuffer;

			TinyRenderObject operator=(const TinyRenderObject&) = delete;
			TinyRenderObject(const TinyRenderObject&) = delete;

			TinyRenderObject(TinyPipeline& pipeline, std::pair<VkCommandBuffer, int32_t>& commandBuffer) : executionPipeline(pipeline), executionBuffer(commandBuffer) {}

			void StageBuffer(TinyBuffer& stageBuffer, TinyBuffer& destBuffer, void* sourceData, VkDeviceSize byteSize, VkDeviceSize& destOffset) {
				void* stagedOffset = static_cast<int8_t*>(stageBuffer.description.pMappedData) + destOffset;
				memcpy(stagedOffset, sourceData, (size_t) byteSize);
				VkBufferCopy copyRegion { .srcOffset = destOffset, .dstOffset = 0, .size = byteSize };
				vkCmdCopyBuffer(executionBuffer.first, stageBuffer.buffer, destBuffer.buffer, 1, &copyRegion);
				destOffset += byteSize;
			}

			void StageImage(TinyBuffer& stageBuffer, TinyImage& destImage, void* sourceData, VkRect2D rect, VkDeviceSize byteSize, VkDeviceSize& destOffset) {
				void* stagedOffset = static_cast<int8_t*>(stageBuffer.description.pMappedData) + destOffset;
				memcpy(stagedOffset, sourceData, (size_t)byteSize);
				
				destImage.TransitionLayoutBarrier(executionBuffer.first, TinyCmdBufferSubmitStage::STAGE_BEGIN, TinyImageLayout::LAYOUT_TRANSFER_DST);
				VkBufferImageCopy region = {
					.imageSubresource.aspectMask = destImage.aspectFlags, .bufferOffset = 0, .bufferRowLength = 0, .bufferImageHeight = 0,
					.imageSubresource.mipLevel = 0, .imageSubresource.baseArrayLayer = 0, .imageSubresource.layerCount = 1,
					.imageExtent = { static_cast<uint32_t>((rect.extent.width == 0)?destImage.width:rect.extent.width),
							static_cast<uint32_t>((rect.extent.height == 0)?destImage.height:rect.extent.height), 1 },
					.imageOffset = { static_cast<int32_t>(rect.offset.x), static_cast<int32_t>(rect.offset.y), 0 },
					.bufferOffset = destOffset,
				};

				vkCmdCopyBufferToImage(executionBuffer.first, stageBuffer.buffer, destImage.image, (VkImageLayout) destImage.imageLayout, 1, &region);
				destImage.TransitionLayoutBarrier(executionBuffer.first, TinyCmdBufferSubmitStage::STAGE_END, TinyImageLayout::LAYOUT_SHADER_READONLY);
				destOffset += byteSize;
			}

			void PushConstant(void* sourceData, TinyShaderStages shaderFlags, VkDeviceSize byteSize) {
				vkCmdPushConstants(executionBuffer.first, executionPipeline.layout, static_cast<VkShaderStageFlagBits>(shaderFlags), 0, byteSize, sourceData);
			}

			void PushBuffer(TinyBuffer& uniformBuffer, VkDeviceSize binding) {
				VkDescriptorBufferInfo bufferDescriptor = uniformBuffer.GetDescriptorInfo();
				VkWriteDescriptorSet bufferDescriptorSet = uniformBuffer.GetWriteDescriptor(0, 1, &bufferDescriptor);
				vkCmdPushDescriptorSetEKHR(executionPipeline.vkdevice.instance, executionBuffer.first, VK_PIPELINE_BIND_POINT_GRAPHICS, executionPipeline.layout, 0, 1, &bufferDescriptorSet);
			}

			void PushImage(TinyImage& uniformImage, VkDeviceSize bindingIndex) {
				VkDescriptorImageInfo imageDescriptor = uniformImage.GetDescriptorInfo();
				VkWriteDescriptorSet imageDescriptorSet = uniformImage.GetWriteDescriptor(0, 1, &imageDescriptor);
				vkCmdPushDescriptorSetEKHR(executionPipeline.vkdevice.instance, executionBuffer.first, VK_PIPELINE_BIND_POINT_GRAPHICS, executionPipeline.layout, 0, 1, &imageDescriptorSet);
			}

			void BindVertices(TinyBuffer& vertexBuffer, VkDeviceSize bindingIndex) {
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(executionBuffer.first, 0, 1, &vertexBuffer.buffer, offsets);
			}
			
			void DrawInstances(VkDeviceSize vertexCount, VkDeviceSize instanceCount, VkDeviceSize firstVertex, VkDeviceSize firstInstance) {
				vkCmdDraw(executionBuffer.first, vertexCount, instanceCount, firstVertex, firstInstance);
			}
		};

        class TinyRenderPass : public TinyDisposable {
        public:
			TinyVkDevice& vkdevice;
			TinyCommandPool& cmdPool;
			TinyPipeline& pipeline;

            TinyImage* targetImage;
			const std::string title;
			const VkDeviceSize subpassIndex;
			const VkDeviceSize localSubpassIndex;
			VkDeviceSize timelineWait;
			VkResult initialized = VK_ERROR_INITIALIZATION_FAILED;
			VkQueryPool timestampQueryPool = VK_NULL_HANDLE;
			uint32_t maxTimestamps, timestampIterator;
			TinyInvokable<TinyRenderPass&, TinyRenderObject&, bool> renderEvent;

			std::vector<TinyRenderPass*> dependencies;
			
			TinyRenderPass operator=(const TinyRenderPass&) = delete;
			TinyRenderPass(const TinyRenderPass&) = delete;
			~TinyRenderPass() { this->Dispose(); }
            
			void Disposable(bool waitIdle) {
				if (waitIdle) vkDeviceWaitIdle(vkdevice.logicalDevice);
				if (timestampQueryPool != VK_NULL_HANDLE) vkDestroyQueryPool(vkdevice.logicalDevice, timestampQueryPool, VK_NULL_HANDLE);
			}

			TinyRenderPass(TinyVkDevice& vkdevice, TinyCommandPool& cmdPool, TinyPipeline& pipeline, TinyImage* targetImage, std::string title, VkDeviceSize subpassIndex, VkDeviceSize localSubpassIndex, uint32_t maxTimestamps = 16U)
			: vkdevice(vkdevice), cmdPool(cmdPool), pipeline(pipeline), title(title), subpassIndex(subpassIndex), localSubpassIndex(localSubpassIndex), timelineWait(0), timestampIterator(0), maxTimestamps(2U * maxTimestamps * TINY_ENGINE_VALIDATION) {
				if (pipeline.createInfo.type == TinyPipelineType::TYPE_GRAPHICS) {
					#if TINY_ENGINE_VALIDATION
						std::cout << "TinyEngine: Created [" << title << "] non-transfer/swapchain renderpass with NULLPOINTER image (image not provided)." << std::endl;
					#endif
				}

				onDispose.hook(TinyCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));
				initialized = VK_SUCCESS;

				#if TINY_ENGINE_VALIDATION
					VkQueryPoolCreateInfo queryCreateInfo = { .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, .queryType = VK_QUERY_TYPE_TIMESTAMP, .queryCount = 2U * maxTimestamps * TINY_ENGINE_VALIDATION, .flags = 0 };
					vkCreateQueryPool(vkdevice.logicalDevice, &queryCreateInfo, VK_NULL_HANDLE, &timestampQueryPool);
				#endif
			}

			VkResult AddDependency(TinyRenderPass& dependency) {
				if (subpassIndex <= dependency.subpassIndex) {
					#if TINY_ENGINE_VALIDATION
						std::cout << "TinyEngine: Tried to create cyclical renderpass dependency: " << subpassIndex << " ID depends " << dependency.subpassIndex << " ID" << std::endl;
						std::cout << "\t\tRender passes cannot have dependency passes initialized before them (self/equal or lower IDs)." << std::endl;
					#endif
					return VK_ERROR_NOT_PERMITTED_KHR;
				}
				
				dependencies.push_back(&dependency);
				timelineWait = std::max(timelineWait, dependency.subpassIndex);
				return VK_SUCCESS;
			}

			std::vector<float> QueryTimeStamps() {
				std::vector<float> frametimes;
				#if TINY_ENGINE_VALIDATION
					if (timestampIterator > 0) {
						std::vector<VkDeviceSize> timestamps(timestampIterator);
						vkGetQueryPoolResults(vkdevice.logicalDevice, timestampQueryPool, 0, timestamps.size(), timestamps.size() * sizeof(VkDeviceSize), timestamps.data(), sizeof(VkDeviceSize), VK_QUERY_RESULT_64_BIT);
						
						for(int i = 0; i < timestampIterator; i += 2) {
							float deltams = float(timestamps[i+1] - timestamps[i]) * (vkdevice.deviceProperties.properties.limits.timestampPeriod / 1000000.0f);
							frametimes.push_back(deltams);
						}
					}
				#endif
				return frametimes;
			}
			
			std::pair<VkCommandBuffer,int32_t> BeginRecordCmdBuffer() {
				std::pair<VkCommandBuffer,int32_t> bufferIndexPair = cmdPool.LeaseBuffer();
				VkCommandBufferBeginInfo beginInfo { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT };
				VkResult result = vkBeginCommandBuffer(bufferIndexPair.first, &beginInfo);

				if (result != VK_SUCCESS) {
					cmdPool.ReturnBuffer(bufferIndexPair);
					return std::pair(VK_NULL_HANDLE, -1);
				}
				
				#if TINY_ENGINE_VALIDATION
					vkCmdResetQueryPool(bufferIndexPair.first, timestampQueryPool, timestampIterator, 2);
					vkCmdWriteTimestamp(bufferIndexPair.first, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestampQueryPool, timestampIterator);
					timestampIterator ++;
				#endif

                targetImage->TransitionLayoutBarrier(bufferIndexPair.first, TinyCmdBufferSubmitStage::STAGE_BEGIN, TinyImageLayout::LAYOUT_COLOR_ATTACHMENT);

				VkViewport dynamicViewportKHR { .x = 0, .y = 0, .minDepth = 0.0f, .maxDepth = 1.0f, .width = static_cast<float>(targetImage->width), .height = static_cast<float>(targetImage->height) };
				vkCmdSetViewport(bufferIndexPair.first, 0, 1, &dynamicViewportKHR);
				
				VkRect2D renderAreaKHR = { .offset = { .x = 0, .y = 0 } , .extent = { .width = static_cast<uint32_t>(targetImage->width), .height = static_cast<uint32_t>(targetImage->height) } };
				/*
				// bool useDefaultRenderArea, VkRect2D renderArea = {}, VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f }
				if (!useDefaultRenderArea) {
					int32_t x2 = static_cast<int32_t>(targetImage->width);
					int32_t y2 = static_cast<int32_t>(targetImage->height);

					renderAreaKHR.offset = {
						.x = std::max(0, std::min(x2, renderArea.offset.x)),
						.y = std::max(0, std::min(y2, renderArea.offset.y)),
					};
					renderAreaKHR.extent = {
						.width = static_cast<uint32_t>(std::max(0, std::min(x2, static_cast<int32_t>(renderArea.extent.width) - renderArea.offset.x))),
						.height = static_cast<uint32_t>(std::max(0, std::min(y2, static_cast<int32_t>(renderArea.extent.height) - renderArea.offset.y)))
					};
				}
				*/
				vkCmdSetScissor(bufferIndexPair.first, 0, 1, &renderAreaKHR);

				VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
				VkRenderingAttachmentInfoKHR colorAttachmentInfo { .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
					.clearValue = clearColor, .loadOp = ((pipeline.createInfo.clearOnLoad)?VK_ATTACHMENT_LOAD_OP_CLEAR:VK_ATTACHMENT_LOAD_OP_DONT_CARE), .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.imageView = targetImage->imageView, .imageLayout = (VkImageLayout) targetImage->imageLayout
				};
				VkRenderingInfoKHR dynamicRenderInfo { .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR, .colorAttachmentCount = 1, .pColorAttachments = &colorAttachmentInfo, .renderArea = renderAreaKHR, .layerCount = 1 };
				result = vkCmdBeginRenderingEKHR(pipeline.vkdevice.instance, bufferIndexPair.first, &dynamicRenderInfo);
				
				if (result != VK_SUCCESS) {
					cmdPool.ReturnBuffer(bufferIndexPair);
					return std::pair(VK_NULL_HANDLE, -1);
				}
				
				vkCmdBindPipeline(bufferIndexPair.first, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
                return bufferIndexPair;
			}
			
			void EndRecordCmdBuffer(std::pair<VkCommandBuffer,int32_t> bufferIndexPair) {
				VkResult result = vkCmdEndRenderingEKHR(pipeline.vkdevice.instance, bufferIndexPair.first);
				targetImage->TransitionLayoutBarrier(bufferIndexPair.first, TinyCmdBufferSubmitStage::STAGE_END,
					(targetImage->imageType == TinyImageType::TYPE_SWAPCHAIN)?
						TinyImageLayout::LAYOUT_PRESENT_SRC : TinyImageLayout::LAYOUT_SHADER_READONLY);
						
				#if TINY_ENGINE_VALIDATION
					vkCmdWriteTimestamp(bufferIndexPair.first, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampQueryPool, timestampIterator);
					timestampIterator ++;
				#endif

				vkEndCommandBuffer(bufferIndexPair.first);
			}
        
			std::pair<VkCommandBuffer, int32_t> BeginStageCmdBuffer() {
				std::pair<VkCommandBuffer, int32_t> bufferIndexPair = cmdPool.LeaseBuffer(false);
				VkCommandBufferBeginInfo beginInfo { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT };
				VkResult result = vkBeginCommandBuffer(bufferIndexPair.first, &beginInfo);
				
				if (result != VK_SUCCESS) {
					cmdPool.ReturnBuffer(bufferIndexPair);
					return std::pair(VK_NULL_HANDLE, -1);
				}
				
				#if TINY_ENGINE_VALIDATION
					vkCmdResetQueryPool(bufferIndexPair.first, timestampQueryPool, timestampIterator, 2);
					vkCmdWriteTimestamp(bufferIndexPair.first, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestampQueryPool, timestampIterator);
					timestampIterator ++;
				#endif

				return bufferIndexPair;
			}
			
			void EndStageCmdBuffer(std::pair<VkCommandBuffer, int32_t> bufferIndexPair) {
				#if TINY_ENGINE_VALIDATION
					vkCmdWriteTimestamp(bufferIndexPair.first, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampQueryPool, timestampIterator);
					timestampIterator ++;
				#endif

				vkEndCommandBuffer(bufferIndexPair.first);
			}
		};
    }
#endif