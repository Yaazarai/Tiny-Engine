#ifndef TINY_ENGINE_TINYBUFFER
#define TINY_ENGINE_TINYBUFFER
	#include "./TinyEngine.hpp"

	namespace TINY_ENGINE_NAMESPACE {
		/// @brief GPU device Buffer for sending data to the render (GPU) device.
		class TinyBuffer : public TinyDisposable {
		public:
			TinyVkDevice& vkdevice;

			VkBuffer buffer = VK_NULL_HANDLE;
			VmaAllocation memory = VK_NULL_HANDLE;
			VmaAllocationInfo description;
			const TinyBufferType bufferType;
			VkDeviceSize size;
			VkResult initialized = VK_ERROR_INITIALIZATION_FAILED;
			
			TinyBuffer operator=(const TinyBuffer&) = delete;
			TinyBuffer(const TinyBuffer&) = delete;
			~TinyBuffer() { this->Dispose(); }

			void Disposable(bool waitIdle) {
				if (waitIdle) vkDeviceWaitIdle(vkdevice.logicalDevice);
				vmaDestroyBuffer(vkdevice.memoryAllocator, buffer, memory);
			}

			TinyBuffer(TinyVkDevice& vkdevice, const TinyBufferType bufferType, VkDeviceSize dataSize)
			: vkdevice(vkdevice), size(dataSize), bufferType(bufferType) {
				onDispose.hook(TinyCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));
				initialized = Initialize();
			}

			VkResult CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaAllocationCreateFlags flags) {
				VkBufferCreateInfo bufCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = usage };
				VmaAllocationCreateInfo allocCreateInfo { .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST, .flags = flags };
				return vmaCreateBuffer(vkdevice.memoryAllocator, &bufCreateInfo, &allocCreateInfo, &buffer, &memory, &description);
			}

			void GetPipelineBarrierStages(TinyCmdBufferSubmitStage cmdBufferStage, VkPipelineStageFlags& srcStage, VkPipelineStageFlags& dstStage, VkAccessFlags& srcAccessMask, VkAccessFlags& dstAccessMask) {
				if (cmdBufferStage == TinyCmdBufferSubmitStage::STAGE_BEGIN) {
					switch(bufferType) {
						case TinyBufferType::TYPE_STAGING:
							srcAccessMask = VK_ACCESS_NONE;
							dstAccessMask = VK_ACCESS_NONE;
							srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
							dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
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
			
			VkDescriptorBufferInfo GetDescriptorInfo(VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE) {
				return { buffer, offset, range };
			}
			
			inline static VkWriteDescriptorSet GetWriteDescriptor(uint32_t binding, uint32_t descriptorCount, const VkDescriptorBufferInfo* bufferInfo) {
				return { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pBufferInfo = bufferInfo, .dstSet = 0, .dstBinding = binding, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = descriptorCount };
			}
			
			VkResult Initialize() {
                switch (bufferType) {
					case TinyBufferType::TYPE_VERTEX:
						return CreateBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
					break;
					case TinyBufferType::TYPE_INDEX:
						return CreateBuffer(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
					break;
					case TinyBufferType::TYPE_UNIFORM:
						return CreateBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
					break;
					case TinyBufferType::TYPE_INDIRECT:
						return CreateBuffer(size, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
					break;
					default: case TinyBufferType::TYPE_STAGING:
						return CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
					break;
				}
			}
		};
	}
#endif