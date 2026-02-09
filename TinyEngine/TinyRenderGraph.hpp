#ifndef TINY_ENGINE_RENDERGRAPH
#define TINY_ENGINE_RENDERGRAPH
	#include "./TinyEngine.hpp"

	namespace TINY_ENGINE_NAMESPACE {
		enum class TinyRenderType {
			STAGING_BUFFER, STAGING_IMAGE, PUSH_CONSTANT, PUSH_DESCRIPTOR, VERTEX_BUFFER, DRAW_COMMAND, RENDER_AREA, CLEAR_COLOR
		};

		struct TinyRenderCmd {
			TinyRenderType type;
			TinyShaderStages stages;
			TinyBuffer* srcBuffer;
			TinyBuffer* dstBuffer;
			TinyImage* srcImage;
			TinyImage* dstImage;
			void* pdata;
			VkDeviceSize values[8];
		};
		
        class TinyRenderObject {
        public:
			std::vector<TinyRenderCmd> commands;

			void StageBuffer(TinyBuffer& stageBuffer, TinyBuffer& destBuffer, void* pdata, VkDeviceSize byteSize, VkDeviceSize& destOffset) {
				TinyRenderCmd cmd = { .type = TinyRenderType::STAGING_BUFFER, .srcBuffer = &stageBuffer, .dstBuffer = &destBuffer, .values[0] = byteSize, .values[1] = destOffset, .pdata = pdata };
				destOffset += byteSize;
				commands.push_back(cmd);
			}

			void StageImage(TinyBuffer& stageBuffer, TinyImage& destImage, void* pdata, VkRect2D rect, VkDeviceSize byteSize, VkDeviceSize& destOffset) {
				TinyRenderCmd cmd = { .type = TinyRenderType::STAGING_IMAGE, .srcBuffer = &stageBuffer, .dstImage = &destImage, .pdata = pdata,
					.values[0] = static_cast<VkDeviceSize>(rect.extent.width), .values[1] = static_cast<VkDeviceSize>(rect.extent.height),
					.values[2] = static_cast<VkDeviceSize>(rect.offset.x), .values[3] = static_cast<VkDeviceSize>(rect.offset.y),
					.values[4] = byteSize,
					.values[5] = destOffset };
				destOffset += byteSize;
				commands.push_back(cmd);
			}

			void PushConstant(TinyShaderStages shaderFlags, void* pdata, VkDeviceSize byteSize) {
				TinyRenderCmd cmd = { .type = TinyRenderType::PUSH_CONSTANT, .stages = shaderFlags, .pdata = pdata, .values[0] = byteSize };
				commands.push_back(cmd);
			}

			void PushBuffer(TinyBuffer& uniformBuffer, VkDeviceSize binding) {
				TinyRenderCmd cmd = { .type = TinyRenderType::PUSH_DESCRIPTOR, .srcBuffer = &uniformBuffer, .values[0] = binding };
				commands.push_back(cmd);
			}

			void PushImage(TinyImage& uniformImage, VkDeviceSize bindingIndex) {
				TinyRenderCmd cmd = { .type = TinyRenderType::PUSH_DESCRIPTOR, .srcImage = &uniformImage, .values[0] = bindingIndex };
				commands.push_back(cmd);
			}

			void BindVertices(TinyBuffer& vertexBuffer, VkDeviceSize bindingIndex) {
				TinyRenderCmd cmd = { .type = TinyRenderType::VERTEX_BUFFER, .srcBuffer = &vertexBuffer, .values[0] = bindingIndex };
				commands.push_back(cmd);
			}
			
			void DrawInstances(VkDeviceSize vertexCount, VkDeviceSize instanceCount, VkDeviceSize firstVertexIndex, VkDeviceSize firstInstance) {
				TinyRenderCmd cmd = { .type = TinyRenderType::DRAW_COMMAND, .values[0] = vertexCount, .values[1] = instanceCount, .values[2] = firstVertexIndex, .values[3] = firstInstance };
				commands.push_back(cmd);
			}

			void SetRenderArea(VkDeviceSize xpos, VkDeviceSize ypos, VkDeviceSize width, VkDeviceSize height) {
				TinyRenderCmd cmd = { .type = TinyRenderType::RENDER_AREA, .values[0] = xpos, .values[1] = ypos, .values[2] = width, .values[3] = height };
				commands.push_back(cmd);
			}

			void SetClearColor(VkDeviceSize rcomp, VkDeviceSize gcomp, VkDeviceSize bcomp, VkDeviceSize acomp) {
				TinyRenderCmd cmd = { .type = TinyRenderType::CLEAR_COLOR, .values[0] = rcomp, .values[1] = gcomp, .values[2] = bcomp, .values[3] = acomp };
				commands.push_back(cmd);
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
			VkDeviceSize timelineWait;
			VkResult initialized = VK_ERROR_INITIALIZATION_FAILED;
			VkQueryPool timestampQueryPool = VK_NULL_HANDLE;
			uint32_t maxTimestamps, timestampIterator;

			std::vector<TinyRenderPass*> dependencies;
			TinyRenderObject renderObject;
			TinyInvokable<TinyRenderPass&, TinyRenderObject&, bool> renderEvent;
			
			TinyRenderPass operator=(const TinyRenderPass&) = delete;
			TinyRenderPass(const TinyRenderPass&) = delete;
			~TinyRenderPass() { this->Dispose(); }
            
			void Disposable(bool waitIdle) {
				if (waitIdle) vkDeviceWaitIdle(vkdevice.logicalDevice);
				if (targetImage != VK_NULL_HANDLE) delete targetImage;
				if (timestampQueryPool != VK_NULL_HANDLE) vkDestroyQueryPool(vkdevice.logicalDevice, timestampQueryPool, VK_NULL_HANDLE);
			}

			TinyRenderPass(TinyVkDevice& vkdevice, TinyCommandPool& cmdPool, TinyPipeline& pipeline, std::string title, VkDeviceSize subpassIndex, VkExtent2D subpassExtent, uint32_t maxTimestamps = 16U)
			: vkdevice(vkdevice), cmdPool(cmdPool), pipeline(pipeline), title(title), subpassIndex(subpassIndex), timelineWait(0), timestampIterator(0), maxTimestamps(2U * maxTimestamps * TINY_ENGINE_VALIDATION) {
				if (pipeline.createInfo.type == TinyPipelineType::TYPE_GRAPHICS) {
					targetImage = new TinyImage(vkdevice, TinyImageType::TYPE_COLORATTACHMENT, subpassExtent.width, subpassExtent.height, pipeline.createInfo.imageFormat, pipeline.createInfo.addressMode, pipeline.createInfo.interpolation);
					targetImage->Initialize();
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
			
			std::pair<VkCommandBuffer,int32_t> BeginRecordCmdBuffer(bool useDefaultRenderArea, VkRect2D renderArea = {}, VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f }) {
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
				vkCmdSetScissor(bufferIndexPair.first, 0, 1, &renderAreaKHR);

				VkRenderingAttachmentInfoKHR colorAttachmentInfo { .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
					.clearValue = clearColor, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
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
		
		using TinyRenderEvent = TinyCallback<TinyRenderPass&, TinyRenderObject&, bool>;

        class TinyRenderGraph : public TinyDisposable {
		public:
			TinyVkDevice& vkdevice;
			TinyWindow* window;

			VkFence swapImageInFlight;
			VkSemaphore swapImageAvailable, swapImageFinished, swapImageTimeline;
            
			std::timed_mutex swapChainMutex;
			TinySurfaceSupporter swapChainPresentDetails;
			VkQueue swapChainPresentQueue;
			VkSwapchainKHR swapChain;
			uint32_t swapFrameIndex;
			std::vector<TinyImage*> swapChainImages;
			std::vector<TinyImage*> resizableImages;

			std::atomic_int64_t frameCounter, renderPassCounter;
			std::atomic_bool presentable, refreshable, frameResized;
			std::vector<TinyRenderPass*> renderPasses;
			VkResult initialized = VK_ERROR_INITIALIZATION_FAILED;
			
			TinyRenderGraph operator=(const TinyRenderGraph&) = delete;
			TinyRenderGraph(const TinyRenderGraph&) = delete;
			~TinyRenderGraph() { this->Dispose(); }

			void Disposable(bool waitIdle) {
				if (waitIdle) vkDeviceWaitIdle(vkdevice.logicalDevice);

				for(TinyImage* swapImage : swapChainImages) {
					vkDestroyImageView(vkdevice.logicalDevice, swapImage->imageView, VK_NULL_HANDLE);
					delete swapImage;
				}
				
				for(TinyRenderPass* pass : renderPasses) delete pass;

				vkDestroySwapchainKHR(vkdevice.logicalDevice, swapChain, VK_NULL_HANDLE);
				vkDestroySemaphore(vkdevice.logicalDevice, swapImageAvailable, VK_NULL_HANDLE);
				vkDestroySemaphore(vkdevice.logicalDevice, swapImageFinished, VK_NULL_HANDLE);
				vkDestroyFence(vkdevice.logicalDevice, swapImageInFlight, VK_NULL_HANDLE);
				vkDestroySemaphore(vkdevice.logicalDevice, swapImageTimeline, VK_NULL_HANDLE);
			}

			TinyRenderGraph(TinyVkDevice& vkdevice, TinyWindow* window, TinySurfaceSupporter swapChainPresentDetails = TinySurfaceSupporter()) : vkdevice(vkdevice), window(window), swapChainPresentDetails(swapChainPresentDetails), presentable(true), refreshable(false), frameResized(false), swapChain(VK_NULL_HANDLE), renderPassCounter(0), frameCounter(0), swapFrameIndex(0) {
				onDispose.hook(TinyCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));
				initialized = Initialize();
			}
			
			void ResizeImageWithSwapchain(TinyImage* resizableImage) {
				bool hasImage = false;

				for(TinyImage* image : swapChainImages)
					if (image == resizableImage) hasImage = true;

				for(TinyImage* image : resizableImages)
					if (image == resizableImage) hasImage = true;
				
				if (resizableImage->imageType != TinyImageType::TYPE_SWAPCHAIN && hasImage == false)
					resizableImages.push_back(resizableImage);
			}

			std::vector<TinyRenderPass*> CreateRenderPass(TinyCommandPool& cmdPool, TinyPipeline& pipeline, std::string title, std::vector<VkExtent2D> extents, VkDeviceSize subpassCount = 1, uint32_t maxTimestamps = 16U) {
				std::vector<TinyRenderPass*> subpasses;
				if (extents.size() == 0) {
					#if TINY_ENGINE_VALIDATION
						std::cout << "Tried to create [" << subpassCount << "] subpasses with zero provided image extents." << std::endl;
					#endif
					return subpasses;
				}

				for(int32_t i = 0; i < std::max(1, static_cast<int32_t>(subpassCount)); i++) {
					size_t index = (extents.size() == static_cast<size_t>(subpassCount))? static_cast<size_t>(i) : 0ULL;
					TinyRenderPass* renderpass = new TinyRenderPass(vkdevice, cmdPool, pipeline, title, renderPassCounter ++, extents[index], maxTimestamps);
					renderPasses.push_back(renderpass);
					subpasses.push_back(renderpass);
                    
					#if TINY_ENGINE_VALIDATION
					switch(renderpass->pipeline.createInfo.type) {
						case TinyPipelineType::TYPE_GRAPHICS:
						std::cout << "TinyEngine: Created graphics pass [" << (renderPassCounter - 1) << ", " << renderpass->title << "]" << std::endl;
						break;
						case TinyPipelineType::TYPE_PRESENT:
						std::cout << "TinyEngine: Created present pass [" << (renderPassCounter - 1) << ", " << renderpass->title << "]" << std::endl;
						break;
						case TinyPipelineType::TYPE_TRANSFER:
						std::cout << "TinyEngine: Created transfer only pass [" << (renderPassCounter - 1) << ", " << renderpass->title << "]" << std::endl;
						break;
					}
					#endif
				}
				return subpasses;
			}
			
			void ResizeFrameBuffer(GLFWwindow* hwndWindow, int width, int height) {
				if (width == 0 || height == 0) return;

				for(TinyImage* swapImage : swapChainImages) {
					vkDestroyImageView(vkdevice.logicalDevice, swapImage->imageView, VK_NULL_HANDLE);
					delete swapImage;
				}

				#if TINY_ENGINE_VALIDATION
					std::cout << "Resizing Window: " << window->hwndWidth << " : " << window->hwndHeight << " -> " << width << " : " << height << std::endl;
				#endif

				for(TinyImage* resizableImage : resizableImages) {
					#if TINY_ENGINE_VALIDATION
						std::cout << "\t" << "Resizing Image: " << resizableImage->width << " : " << resizableImage->height << " -> " << width << " : " << height << std::endl;
					#endif
					resizableImage->Disposable(false);
					resizableImage->CreateImage(resizableImage->imageType, width, height, resizableImage->imageFormat, resizableImage->addressMode, resizableImage->interpolation);
				}
				
				VkSwapchainKHR oldSwapChain = swapChain;
				TinySwapchain::CreateSwapChainImages(vkdevice, *window, swapChainPresentDetails, swapChain, swapChainImages);
				TinySwapchain::CreateSwapChainImageViews(vkdevice, swapChainPresentDetails, swapChainImages);
				vkDestroySwapchainKHR(vkdevice.logicalDevice, oldSwapChain, VK_NULL_HANDLE);

				presentable = true;
				refreshable = false;
				frameResized = true;
			}
			
			VkResult ExecuteRenderGraph() {
				for(TinyRenderPass* pass : renderPasses) {
					pass->cmdPool.ReturnAllBuffers();
					pass->timestampIterator = 0;
					pass->renderObject.commands.clear();
				}
				
				VkResult result = VK_SUCCESS;
				for(int32_t i = 0; i < renderPasses.size(); i++) {
					if (renderPasses[i]->pipeline.createInfo.type == TinyPipelineType::TYPE_PRESENT)
						renderPasses[i]->targetImage = swapChainImages[swapFrameIndex];
					
					////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
					std::vector<VkCommandBuffer> cmdbuffers;
					renderPasses[i]->renderEvent.invoke(*renderPasses[i], renderPasses[i]->renderObject, static_cast<bool>(frameResized));

					VkRect2D renderArea = {};
					bool useDefaultRenderArea = true;

					VkClearValue renderColor = { 0.0f, 0.0f, 0.0f, 1.0f };

					for(TinyRenderCmd rendercmd : renderPasses[i]->renderObject.commands) {
						switch(rendercmd.type) {
							case TinyRenderType::RENDER_AREA:
								{
									useDefaultRenderArea = false;
									renderArea = { .offset.x = static_cast<int32_t>(rendercmd.values[0]), .offset.y = static_cast<int32_t>(rendercmd.values[1]), .extent.width = static_cast<uint32_t>(rendercmd.values[2]), .extent.height = static_cast<uint32_t>(rendercmd.values[3]) };
								}
							break;
							case TinyRenderType::CLEAR_COLOR:
								{
									renderColor.color = {
										static_cast<float>(rendercmd.values[0]) / 255.0f,
										static_cast<float>(rendercmd.values[1]) / 255.0f,
										static_cast<float>(rendercmd.values[2]) / 255.0f,
										static_cast<float>(rendercmd.values[3]) / 255.0f
									};
								}
							break;
						}
					}
					
					std::pair<VkCommandBuffer, int32_t> cmdbufferPair = (renderPasses[i]->pipeline.createInfo.type == TinyPipelineType::TYPE_TRANSFER)?
						renderPasses[i]->BeginStageCmdBuffer() : renderPasses[i]->BeginRecordCmdBuffer(useDefaultRenderArea, renderArea, renderColor);
					
					for(TinyRenderCmd rendercmd : renderPasses[i]->renderObject.commands) {
						switch(rendercmd.type) {
							case TinyRenderType::STAGING_BUFFER:
								{
									TinyBuffer& stagedBuffer = *rendercmd.srcBuffer;
									TinyBuffer& destBuffer = *rendercmd.dstBuffer;
									VkDeviceSize sourceSize = rendercmd.values[0];
									VkDeviceSize destOffset = rendercmd.values[1];
									void* sourceData = rendercmd.pdata;

									void* stagedOffset = static_cast<int8_t*>(stagedBuffer.description.pMappedData) + destOffset;
									memcpy(stagedOffset, sourceData, (size_t)sourceSize);
									VkBufferCopy copyRegion { .srcOffset = destOffset, .dstOffset = 0, .size = sourceSize };
									vkCmdCopyBuffer(cmdbufferPair.first, stagedBuffer.buffer, destBuffer.buffer, 1, &copyRegion);
								}
							break;
							case TinyRenderType::STAGING_IMAGE:
								{
									TinyBuffer& stagedBuffer = *rendercmd.srcBuffer;
									TinyImage& destImage = *rendercmd.dstImage;
									VkDeviceSize imageWidth = rendercmd.values[0];
									VkDeviceSize imageHeight = rendercmd.values[1];
									VkDeviceSize imageYoffset = rendercmd.values[2];
									VkDeviceSize imageXoffset = rendercmd.values[3];
									VkDeviceSize sourceSize = rendercmd.values[4];
									VkDeviceSize destOffset = rendercmd.values[5];
									void* sourceData = rendercmd.pdata;

									void* stagedOffset = static_cast<int8_t*>(stagedBuffer.description.pMappedData) + destOffset;
									memcpy(stagedOffset, sourceData, (size_t)sourceSize);
									
									destImage.TransitionLayoutBarrier(cmdbufferPair.first, TinyCmdBufferSubmitStage::STAGE_BEGIN, TinyImageLayout::LAYOUT_TRANSFER_DST);
									VkBufferImageCopy region = {
										.imageSubresource.aspectMask = destImage.aspectFlags, .bufferOffset = 0, .bufferRowLength = 0, .bufferImageHeight = 0,
										.imageSubresource.mipLevel = 0, .imageSubresource.baseArrayLayer = 0, .imageSubresource.layerCount = 1,
										.imageExtent = { static_cast<uint32_t>((imageWidth == 0)?destImage.width:imageWidth), static_cast<uint32_t>((imageHeight == 0)?destImage.height:imageHeight), 1 },
										.imageOffset = { static_cast<int32_t>(imageXoffset), static_cast<int32_t>(imageYoffset), 0 },
										.bufferOffset = destOffset,
									};

									vkCmdCopyBufferToImage(cmdbufferPair.first, stagedBuffer.buffer, destImage.image, (VkImageLayout) destImage.imageLayout, 1, &region);
									destImage.TransitionLayoutBarrier(cmdbufferPair.first, TinyCmdBufferSubmitStage::STAGE_END, TinyImageLayout::LAYOUT_SHADER_READONLY);
								}
							break;
							case TinyRenderType::PUSH_CONSTANT:
								vkCmdPushConstants(cmdbufferPair.first, renderPasses[i]->pipeline.layout, (VkShaderStageFlagBits) rendercmd.stages, 0, rendercmd.values[0], rendercmd.pdata);
							break;
							case TinyRenderType::PUSH_DESCRIPTOR:
								if (rendercmd.srcImage != VK_NULL_HANDLE) {
									VkDescriptorImageInfo imageDescriptor = rendercmd.srcImage->GetDescriptorInfo();
									VkWriteDescriptorSet imageDescriptorSet = rendercmd.srcImage->GetWriteDescriptor(0, 1, &imageDescriptor);
									vkCmdPushDescriptorSetEKHR(renderPasses[i]->pipeline.vkdevice.instance, cmdbufferPair.first, VK_PIPELINE_BIND_POINT_GRAPHICS, renderPasses[i]->pipeline.layout, 0, 1, &imageDescriptorSet);
								} else if (rendercmd.srcBuffer != VK_NULL_HANDLE) {
									VkDescriptorBufferInfo bufferDescriptor = rendercmd.srcBuffer->GetDescriptorInfo();
									VkWriteDescriptorSet bufferDescriptorSet = rendercmd.srcBuffer ->GetWriteDescriptor(0, 1, &bufferDescriptor);
									vkCmdPushDescriptorSetEKHR(renderPasses[i]->pipeline.vkdevice.instance, cmdbufferPair.first, VK_PIPELINE_BIND_POINT_GRAPHICS, renderPasses[i]->pipeline.layout, 0, 1, &bufferDescriptorSet);
								}
							break;
							case TinyRenderType::VERTEX_BUFFER:
								{
									VkDeviceSize offsets[] = { 0 };
									vkCmdBindVertexBuffers(cmdbufferPair.first, 0, 1, &rendercmd.srcBuffer->buffer, offsets);
								}
							break;
							case TinyRenderType::DRAW_COMMAND:
							 	vkCmdDraw(cmdbufferPair.first, rendercmd.values[0], rendercmd.values[1], rendercmd.values[2], rendercmd.values[3]);
							break;
						}
					}

					if (renderPasses[i]->pipeline.createInfo.type == TinyPipelineType::TYPE_TRANSFER) {
						renderPasses[i]->EndStageCmdBuffer(cmdbufferPair);
					} else { renderPasses[i]->EndRecordCmdBuffer(cmdbufferPair); }
					cmdbuffers.push_back(cmdbufferPair.first);

					////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

					VkDeviceSize frameWait = frameCounter * 100;
					VkDeviceSize waitValue = frameWait + renderPasses[i]->timelineWait;
					VkDeviceSize signalValue = frameWait + renderPasses[i]->subpassIndex;
					VkSemaphore initialWaits[] = { swapImageAvailable };
					VkSemaphore dependencyWaits[] { swapImageTimeline };
					VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
					bool isInitialPass = i == 0 || renderPasses[i]->dependencies.size() == 0;
					
					VkSubmitInfo submitInfo { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = static_cast<uint32_t>(cmdbuffers.size()), .pCommandBuffers = cmdbuffers.data(),
						.pWaitDstStageMask = waitStages, .waitSemaphoreCount = static_cast<uint32_t>(isInitialPass), .pWaitSemaphores = (isInitialPass)? initialWaits : dependencyWaits };
					
					VkTimelineSemaphoreSubmitInfo timelineInfo = { .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
						.waitSemaphoreValueCount = 1, .pWaitSemaphoreValues = &waitValue, .signalSemaphoreValueCount = 1, .pSignalSemaphoreValues = &signalValue };
					
					submitInfo.pNext = &timelineInfo;

					if (renderPasses[i]->pipeline.createInfo.type == TinyPipelineType::TYPE_PRESENT) {
						VkSemaphore signalSemaphores[] = {  swapImageFinished };
						submitInfo.signalSemaphoreCount = 1;
						submitInfo.pSignalSemaphores = signalSemaphores;
						result = vkQueueSubmit(swapChainPresentQueue, 1, &submitInfo, swapImageInFlight);
					} else {
						result = vkQueueSubmit(renderPasses[i]->pipeline.submitQueue, 1, &submitInfo, VK_NULL_HANDLE);
					}
				}

				return result;
			}

			VkResult RenderSwapChain() {
				VkResult result = VK_NOT_READY;
				if (!presentable || refreshable) {
					ResizeFrameBuffer(window->hwndWindow, window->hwndWidth, window->hwndHeight);
				} else {
					// Double wait to avoid synch-overlap causing command buffers to still be in use from last present...
					TinySwapchain::WaitResetFences(vkdevice, &swapImageInFlight);
					VkResult result = TinySwapchain::QueryNextSwapChainImage(vkdevice, swapChain, swapFrameIndex, swapImageInFlight, swapImageAvailable);
					TinySwapchain::WaitResetFences(vkdevice, &swapImageInFlight);
					
					if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
						result = ExecuteRenderGraph();
					
					if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
						result = TinySwapchain::QueuePresent(swapChainPresentQueue, swapChain, swapImageFinished, swapFrameIndex);
					
					presentable = (result == VK_SUCCESS);
					frameResized = false;
					frameCounter ++;
				}

				return result;
			}
			
			VkResult Initialize() {
				if (window != VK_NULL_HANDLE) {
					if (!vkdevice.queueFamilyIndices.hasPresentFamily) return VK_ERROR_INITIALIZATION_FAILED;
					vkGetDeviceQueue(vkdevice.logicalDevice, vkdevice.queueFamilyIndices.presentFamily, 0, &swapChainPresentQueue);
					
					TinySwapchain::CreateSwapChainImages(vkdevice, *window, swapChainPresentDetails, swapChain, swapChainImages);
					TinySwapchain::CreateSwapChainImageViews(vkdevice, swapChainPresentDetails, swapChainImages);

					/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
					VkSemaphoreCreateInfo semaphoreCreateInfo { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
					VkFenceCreateInfo fenceCreateInfo { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };

					vkCreateSemaphore(vkdevice.logicalDevice, &semaphoreCreateInfo, VK_NULL_HANDLE, &swapImageAvailable);
					vkCreateSemaphore(vkdevice.logicalDevice, &semaphoreCreateInfo, VK_NULL_HANDLE, &swapImageFinished);
					vkCreateFence(vkdevice.logicalDevice, &fenceCreateInfo, VK_NULL_HANDLE, &swapImageInFlight);
					/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

					VkSemaphoreTypeCreateInfo timelineCreateInfo = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, .pNext = NULL, .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE, .initialValue = 0 };
					VkSemaphoreCreateInfo createInfo = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = &timelineCreateInfo, .flags = 0 };
					vkCreateSemaphore(vkdevice.logicalDevice, &createInfo, NULL, &swapImageTimeline);
				}

				return VK_SUCCESS;
			}
		};
    }
#endif
/*
    Forward-Only Render Graph

    * Forward-Only -> Specifies that renderpasses can rely on other renderpasses
    as dependencies, but only if those dependencies were created early in the
    renderpass chain. This is to avoid circular dependencies.

    * Render Graph -> Specifies the model of automatic renderpass and swapchain
    present synchronization.
*/