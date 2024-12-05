#ifndef TINY_ENGINE_TINYBUFFER
#define TINY_ENGINE_TINYBUFFER
	#include "./TinyEngine.hpp"

	namespace TINY_ENGINE_NAMESPACE {
		/// @brief GPU device Buffer for sending data to the render (GPU) device.
		class TinyBuffer : public TinyDisposable {		
		public:
			std::timed_mutex buffer_lock;
			const TinyBufferType bufferType;
			TinyRenderContext& renderContext;
			VkBuffer buffer = VK_NULL_HANDLE;
			VmaAllocation memory = VK_NULL_HANDLE;
			VmaAllocationInfo description;
			VkDeviceSize size;
			VkFence bufferWaitable;

			/// @brief Deleted copy constructor (dynamic objects are not copyable).
			TinyBuffer operator=(const TinyBuffer&) = delete;
			
			/// @brief Deleted copy constructor (dynamic objects are not copyable).
			TinyBuffer(const TinyBuffer&) = delete;
			
			/// @brief Calls the disposable interface dispose event.
			~TinyBuffer() { this->Dispose(); }

			/// @brief Manually calls dispose on resources without deleting the object.
			void Disposable(bool waitIdle) {
				if (waitIdle) renderContext.vkdevice.DeviceWaitIdle();

				vmaDestroyBuffer(renderContext.vkdevice.memoryAllocator, buffer, memory);
				vkDestroyFence(renderContext.vkdevice.logicalDevice, bufferWaitable, VK_NULL_HANDLE);
			}

			/// @brief Creates a VkBuffer of the specified size in bytes with auto-set memory allocation properties by TinyBufferType.
			TinyBuffer(TinyRenderContext& renderContext, TinyBufferType type, VkDeviceSize dataSize)
			: renderContext(renderContext), size(dataSize), bufferType(type) {
				onDispose.hook(TinyCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));
			}

			/// @brief Create and map this data buffer to GPU memory (auto-called by Initialize() or Construct() or Constructor).
			VkResult CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaAllocationCreateFlags flags) {
				VkBufferCreateInfo bufCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = usage };
				VmaAllocationCreateInfo allocCreateInfo { .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST, .flags = flags };
				return vmaCreateBuffer(renderContext.vkdevice.memoryAllocator, &bufCreateInfo, &allocCreateInfo, &buffer, &memory, &description);
			}

			/// @brief Begins a transfer command and returns the command buffer index pair used for the command allocated from a TinyCommandPool.
			std::pair<VkCommandBuffer, int32_t> BeginTransferCmd() {
				std::pair<VkCommandBuffer, int32_t> bufferIndexPair = renderContext.commandPool.LeaseBuffer(true);
				
				VkCommandBufferBeginInfo beginInfo {
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT	
				};
				vkBeginCommandBuffer(bufferIndexPair.first, &beginInfo);
				return bufferIndexPair;
			}

			/// @brief Ends a transfer command and gives the leased/rented command buffer pair back to the TinyCommandPool.
			void EndTransferCmd(std::pair<VkCommandBuffer, int32_t> bufferIndexPair) {
				vkEndCommandBuffer(bufferIndexPair.first);
				
				VkSubmitInfo submitInfo { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .pCommandBuffers = &bufferIndexPair.first, .commandBufferCount = 1, };
				vkQueueSubmit(renderContext.graphicsPipeline.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
				vkQueueWaitIdle(renderContext.graphicsPipeline.graphicsQueue);
				
				vkResetCommandBuffer(bufferIndexPair.first, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
				renderContext.commandPool.ReturnBuffer(bufferIndexPair);
			}

			/// @brief Copies data from the source TinyBuffer into this TinyBuffer.
			void TransferBufferCmd(TinyRenderContext& renderContext, TinyBuffer& srcBuffer, TinyBuffer& dstBuffer, VkDeviceSize dataSize, VkDeviceSize srceOffset = 0, VkDeviceSize destOffset = 0) {
				std::pair<VkCommandBuffer,int32_t> bufferIndexPair = BeginTransferCmd();

				VkBufferCopy copyRegion { .srcOffset = srceOffset, .dstOffset = destOffset, .size = dataSize };
				vkCmdCopyBuffer(bufferIndexPair.first, srcBuffer.buffer, dstBuffer.buffer, 1, &copyRegion);

				EndTransferCmd(bufferIndexPair);
			}

			/// @brief Copies data from CPU accessible memory to GPU accessible memory.
			void StageBufferData(void* data, VkDeviceSize dataSize, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0) {
				TinyObject<TinyBuffer> stagingBuffer = TinyBuffer::Construct(renderContext, TinyBufferType::TYPE_STAGING, dataSize);
				memcpy(stagingBuffer.ref().description.pMappedData, data, (size_t)dataSize);
				TransferBufferCmd(renderContext, stagingBuffer, *this, size, srcOffset, dstOffset);
			}

			/// @brief Get pipeline stages relative to the current image layout and command buffer recording stage.
			void GetPipelineBarrierStages(TinyCmdBufferSubmitStage cmdBufferStage, VkPipelineStageFlags& srcStage, VkPipelineStageFlags& dstStage, VkAccessFlags& srcAccessMask, VkAccessFlags& dstAccessMask) {
				if (cmdBufferStage == TinyCmdBufferSubmitStage::STAGE_BEGIN) {
					switch(bufferType) {
						case TinyBufferType::TYPE_STAGING:
							srcAccessMask = VK_ACCESS_NONE;
							dstAccessMask = VK_ACCESS_NONE;
							srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
							dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
						break;
						case TinyBufferType::TYPE_STORAGE:
							srcAccessMask = VK_ACCESS_NONE;
							dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
							srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
							dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
						break;
						case TinyBufferType::TYPE_VERTEX:
						case TinyBufferType::TYPE_INDEX:
							srcAccessMask = VK_ACCESS_NONE;
							dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
							srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
							dstStage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
						break;
						case TinyBufferType::TYPE_UNIFORM:
							srcAccessMask = VK_ACCESS_NONE;
							dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
							srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
							dstStage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
						break;
						case TinyBufferType::TYPE_INDIRECT:
						default:
							srcAccessMask = VK_ACCESS_NONE;
							dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
							srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
							dstStage = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
						break;
					}
				} else if (cmdBufferStage == TinyCmdBufferSubmitStage::STAGE_END) {
					switch(bufferType) {
						case TinyBufferType::TYPE_STAGING:
							srcAccessMask = VK_ACCESS_NONE;
							dstAccessMask = VK_ACCESS_NONE;
							srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
							dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
						break;
						case TinyBufferType::TYPE_STORAGE:
							srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
							dstAccessMask = VK_ACCESS_NONE;
							srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
							dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
						break;
						case TinyBufferType::TYPE_VERTEX:
						case TinyBufferType::TYPE_INDEX:
							srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
							dstAccessMask = VK_ACCESS_NONE;
							srcStage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
							dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
						break;
						case TinyBufferType::TYPE_UNIFORM:
							srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
							dstAccessMask = VK_ACCESS_NONE;
							srcStage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
							dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
						break;
						case TinyBufferType::TYPE_INDIRECT:
						default:
							srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
							dstAccessMask = VK_ACCESS_NONE;
							srcStage = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
							dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
						break;
					}
				} else if (cmdBufferStage == TinyCmdBufferSubmitStage::STAGE_BEGIN_TO_END) {
					srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
					dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
					srcAccessMask = VK_ACCESS_NONE;
					dstAccessMask = VK_ACCESS_NONE;
				}
			}
			
			/// @brief Get the pipeline barrier info for resource synchronization in buffer pipeline barrier pNext chain.
			VkBufferMemoryBarrier GetPipelineBarrier(TinyCmdBufferSubmitStage cmdBufferStage, VkPipelineStageFlags& srcStage, VkPipelineStageFlags& dstStage) {
				VkBufferMemoryBarrier pipelineBarrier {
					.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
					.srcAccessMask = VK_ACCESS_NONE, .dstAccessMask = VK_ACCESS_NONE,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.buffer = buffer, .size = VK_WHOLE_SIZE, .offset = 0,
				};

				GetPipelineBarrierStages(cmdBufferStage, srcStage, dstStage, pipelineBarrier.srcAccessMask, pipelineBarrier.dstAccessMask);
				return pipelineBarrier;
			}

			/// @brief Get the pipeline barrier info and submit call of vkCmdPipelineBarrier to VkCommandBuffer.
			void MemoryPipelineBarrier(VkCommandBuffer cmdBuffer, TinyCmdBufferSubmitStage cmdBufferStage) {
				VkPipelineStageFlags srcStage, dstStage;
				VkBufferMemoryBarrier pipelineBarrier = GetPipelineBarrier(cmdBufferStage, srcStage, dstStage);
				vkCmdPipelineBarrier(cmdBuffer, srcStage, dstStage, 0, 0, VK_NULL_HANDLE, 1, &pipelineBarrier, 0, VK_NULL_HANDLE);
			}

			/// @brief Creates the data descriptor that represents this buffer when passing into graphicspipeline.SelectWrite*Descriptor().
			VkDescriptorBufferInfo GetBufferDescriptor(VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE) { return { buffer, offset, range }; }
			
			/// @brief Create the data buffer and map it to GPU memory.
			VkResult Initialize() {
				VkResult result = VK_SUCCESS;
                switch (bufferType) {
					case TinyBufferType::TYPE_VERTEX:
						result = CreateBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
					break;
					case TinyBufferType::TYPE_INDEX:
						result = CreateBuffer(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
					break;
					case TinyBufferType::TYPE_UNIFORM:
						result = CreateBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
					break;
					case TinyBufferType::TYPE_INDIRECT:
						result = CreateBuffer(size, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
					break;
					case TinyBufferType::TYPE_STORAGE:
						result = CreateBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
					break;
					case TinyBufferType::TYPE_STAGING:
					default:
						result = CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
					break;
				}

                if (result == VK_SUCCESS) {
                    VkFenceCreateInfo fenceInfo{
						.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT
					};
                    return vkCreateFence(renderContext.vkdevice.logicalDevice, &fenceInfo, VK_NULL_HANDLE, &bufferWaitable);
                }
					
                return result;
			}
			
			/// @brief Constructor(...) + Initialize() with error result as combined TinyObject<Object,VkResult>.
			template<typename... A>
			inline static TinyObject<TinyBuffer> Construct(TinyRenderContext& renderContext, TinyBufferType type, VkDeviceSize dataSize) {
				std::unique_ptr<TinyBuffer> object =
					std::make_unique<TinyBuffer>(renderContext, type, dataSize);
				return TinyObject<TinyBuffer>(object, object->Initialize());
			}
		};
	}
#endif

///
///	ABVOUT BUFFERS & IMAGES:
///		When Creating Buffers:
///			Buffers must be initialized with a VkDviceSize, which is the size of the data in BYTES, not the
///			size of the data container (number of items). This same principle applies to stagging buffer data.
///
///		There are 3 types of GPU dedicated memory buffers:
///			Vertex:		Allows you to send mesh triangle data to the GPU.
///			Index:		Allws you to send mapped indices for vertex buffers to the GPU.
///			Uniform:	Allows you to send data to shaders using uniforms.
///				* Push Constants are an alternative that do not require buffers, simply use: vkCmdPushConstants(...).
///
///		The last buffer type is a CPU memory buffer for transfering data from the CPU to the GPU:
///			Staging:	Staging CPU data for transfer to the GPU.
///
///		Render images are for rendering sprites or textures on the GPU (similar to the swap chain, but handled manually).
///			The default image layout is: VK_IMAGE_LAYOUT_UNDEFINED
///			To render to shaders you must change/transition the layout to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL.
///			Once the layout is set for transfering you can write data to the image from CPU memory to GPU memory.
///			Finally for use in shaders you need to change the layout to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
///