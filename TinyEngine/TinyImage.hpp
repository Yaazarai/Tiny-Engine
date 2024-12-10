#ifndef TINY_ENGINE_TINYIMAGE
#define TINY_ENGINE_TINYIMAGE
	#include "./TinyEngine.hpp"

	namespace TINY_ENGINE_NAMESPACE {
		/// @brief GPU device image for sending images to the render (GPU) device.
		class TinyImage : public TinyDisposable {
		public:
			TinyRenderContext& renderContext;
			std::timed_mutex image_lock;

			VmaAllocation memory = VK_NULL_HANDLE;
			VkImage image = VK_NULL_HANDLE;
			VkImageView imageView = VK_NULL_HANDLE;
			VkSampler imageSampler = VK_NULL_HANDLE;
			TinyImageLayout imageLayout;
			VkImageAspectFlags aspectFlags;
			VkSamplerAddressMode addressingMode;
			bool textureInterpolation;

			VkSemaphore imageAvailable;
			VkSemaphore imageFinished;
			VkFence imageWaitable;

			VkDeviceSize width, height;
			VkFormat format;
			const TinyImageType imageType;

			/// @brief Deleted copy constructor (dynamic objects are not copyable).
			TinyImage operator=(const TinyImage&) = delete;
			
			/// @brief Deleted copy constructor (dynamic objects are not copyable).
			TinyImage(const TinyImage&) = delete;
			
			/// @brief Calls the disposable interface dispose event.
			~TinyImage() { this->Dispose(); }

			/// @brief Manually calls dispose on resources without deleting the object.
			void Disposable(bool waitIdle) {
				if (waitIdle) renderContext.vkdevice.DeviceWaitIdle();
				
				if (imageType != TinyImageType::TYPE_SWAPCHAIN) {
					vkDestroySampler(renderContext.vkdevice.logicalDevice, imageSampler, VK_NULL_HANDLE);
					vkDestroyImageView(renderContext.vkdevice.logicalDevice, imageView, VK_NULL_HANDLE);
					vmaDestroyImage(renderContext.vkdevice.memoryAllocator, image, memory);
					vkDestroySemaphore(renderContext.vkdevice.logicalDevice, imageAvailable, VK_NULL_HANDLE);
					vkDestroySemaphore(renderContext.vkdevice.logicalDevice, imageFinished, VK_NULL_HANDLE);
					vkDestroyFence(renderContext.vkdevice.logicalDevice, imageWaitable, VK_NULL_HANDLE);
				}
			}

			/// @brief Creates a VkImage for rendering or loading image files (stagedata) into.
			TinyImage(TinyRenderContext& renderContext, TinyImageType type, VkDeviceSize width, VkDeviceSize height, VkFormat format = VK_FORMAT_B8G8R8A8_UNORM, VkSamplerAddressMode addressingMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, bool textureInterpolation = false, VkImage imageSource = VK_NULL_HANDLE, VkImageView imageViewSource = VK_NULL_HANDLE, VkSampler imageSampler = VK_NULL_HANDLE, VkSemaphore imageAvailable = VK_NULL_HANDLE, VkSemaphore imageFinished = VK_NULL_HANDLE, VkFence imageWaitable = VK_NULL_HANDLE)
			: renderContext(renderContext), imageType(type), width(width), height(height), image(imageSource), imageView(imageViewSource), imageSampler(imageSampler), imageAvailable(imageAvailable), imageFinished(imageFinished), imageWaitable(imageWaitable), format(format), imageLayout(TinyImageLayout::LAYOUT_UNDEFINED), addressingMode(addressingMode), textureInterpolation(textureInterpolation), aspectFlags(aspectFlags) {
				onDispose.hook(TinyCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));
			}

			VkResult CreateImageView() {
				VkImageViewCreateInfo createInfo {
					.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
					.image = image, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = format, .components = { VK_COMPONENT_SWIZZLE_IDENTITY },
					.subresourceRange = { .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1, .aspectMask = aspectFlags, }
				};

				return vkCreateImageView(renderContext.vkdevice.logicalDevice, &createInfo, VK_NULL_HANDLE, &imageView);
			}

			VkResult CreateTextureSampler() {
				VkPhysicalDeviceProperties properties {};
				vkGetPhysicalDeviceProperties(renderContext.vkdevice.physicalDevice, &properties);

				VkFilter filter = (textureInterpolation == true)? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
				VkSamplerMipmapMode mipmapMode = (textureInterpolation)? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
				float interpolationWeight = (textureInterpolation)? VK_LOD_CLAMP_NONE : 0.0f;

				VkSamplerCreateInfo samplerInfo {
					.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
					.magFilter = filter, .minFilter = filter,
					.anisotropyEnable = VK_FALSE, .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
					.addressModeU = addressingMode, .addressModeV = addressingMode, .addressModeW = addressingMode, .unnormalizedCoordinates = VK_FALSE,
					.compareEnable = VK_FALSE, .compareOp = VK_COMPARE_OP_ALWAYS,
					.mipmapMode = mipmapMode, .mipLodBias = 0.0f, .minLod = 0.0f, .maxLod = interpolationWeight,
					.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
				};

				return vkCreateSampler(renderContext.vkdevice.logicalDevice, &samplerInfo, VK_NULL_HANDLE, &imageSampler);
			}

			VkResult CreateImageSyncObjects() {
				VkSemaphoreCreateInfo semaphoreInfo { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .flags = VK_SEMAPHORE_TYPE_BINARY };
				VkFenceCreateInfo fenceInfo { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };

				VkResult result = vkCreateSemaphore(renderContext.vkdevice.logicalDevice, &semaphoreInfo, VK_NULL_HANDLE, &imageAvailable);
				if (result != VK_SUCCESS) return result;
				result = vkCreateSemaphore(renderContext.vkdevice.logicalDevice, &semaphoreInfo, VK_NULL_HANDLE, &imageFinished);
				if (result != VK_SUCCESS) return result;
				result = vkCreateFence(renderContext.vkdevice.logicalDevice, &fenceInfo, VK_NULL_HANDLE, &imageWaitable);
				return result;
			}
            
            /// @brief Recreates this TinyImage using a new layout/format (don't forget to call image.Disposable(bool waitIdle) to dispose of the previous image first.
			VkResult ReCreateImage(TinyImageType type, VkDeviceSize width, VkDeviceSize height, VkFormat format = VK_FORMAT_R16G16B16A16_UNORM, VkSamplerAddressMode addressingMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, bool textureInterpolation = false) {
				if (type == TinyImageType::TYPE_SWAPCHAIN)
					return VK_ERROR_INITIALIZATION_FAILED;

				VkImageCreateInfo imgCreateInfo = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.extent.width = static_cast<uint32_t>(width), .extent.height = static_cast<uint32_t>(height),
					.extent.depth = 1, .mipLevels = 1, .arrayLayers = 1,
					.format = format, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, .imageType = VK_IMAGE_TYPE_2D,
					.tiling = VK_IMAGE_TILING_OPTIMAL, .samples = VK_SAMPLE_COUNT_1_BIT,
					.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
				};

				this->width = width;
				this->height = height;
				imageLayout = TinyImageLayout::LAYOUT_UNDEFINED;

				TinyImageLayout newLayout;
				switch(imageType) {
					case TinyImageType::TYPE_DEPTHSTENCIL:
						newLayout = TinyImageLayout::LAYOUT_DEPTHSTENCIL_ATTACHMENT;
						imgCreateInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
						aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
						if (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT)
							aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
					break;
					case TinyImageType::TYPE_STORAGE:
						newLayout = TinyImageLayout::LAYOUT_GENERAL;
						imgCreateInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
						aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
					break;
					case TinyImageType::TYPE_COLORATTACHMENT:
						newLayout = TinyImageLayout::LAYOUT_COLOR_ATTACHMENT;
						imgCreateInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
						aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
					break;
					case TinyImageType::TYPE_SHADER_READONLY:
						newLayout = TinyImageLayout::LAYOUT_SHADER_READONLY;
						imgCreateInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
						aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
					break;
				}

				VmaAllocationCreateInfo allocCreateInfo {};
				allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
				allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
				allocCreateInfo.priority = 1.0f;

				this->textureInterpolation = textureInterpolation;
				
				VkResult result = vmaCreateImage(renderContext.vkdevice.memoryAllocator, &imgCreateInfo, &allocCreateInfo, &image, &memory, VK_NULL_HANDLE);
				if (result != VK_SUCCESS) return result;
				result = CreateTextureSampler();
                if (result != VK_SUCCESS) return result;
				result = CreateImageView();
                if (result != VK_SUCCESS) return result;
				result = CreateImageSyncObjects();
                if (result != VK_SUCCESS) return result;
				
				if (newLayout != TinyImageLayout::LAYOUT_UNDEFINED)
					TransitionLayoutCmd(newLayout);
				
				return VK_SUCCESS;
			}
			
			/// @brief Get pipeline stages relative to the current image layout and command buffer recording stage.
			void GetPipelineBarrierStages(TinyImageLayout layout, TinyCmdBufferSubmitStage cmdBufferStage, VkPipelineStageFlags& srcStage, VkPipelineStageFlags& dstStage, VkAccessFlags& srcAccessMask, VkAccessFlags& dstAccessMask) {
				if (cmdBufferStage == TinyCmdBufferSubmitStage::STAGE_BEGIN) {
					switch (layout) {
						case TinyImageLayout::LAYOUT_COLOR_ATTACHMENT:
							srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
							dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
							srcAccessMask = VK_ACCESS_NONE;
							dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
						break;
						case TinyImageLayout::LAYOUT_PRESENT_SRC:
							srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
							dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
							srcAccessMask = VK_ACCESS_NONE;
							dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
						break;
						case TinyImageLayout::LAYOUT_TRANSFER_SRC:
							srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
							dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
							srcAccessMask = VK_ACCESS_NONE;
							dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
						break;
						case TinyImageLayout::LAYOUT_TRANSFER_DST:
							srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
							dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
							srcAccessMask = VK_ACCESS_NONE;
							dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
						break;
						case TinyImageLayout::LAYOUT_SHADER_READONLY:
							srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
							dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
							srcAccessMask = VK_ACCESS_NONE;
							dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
						break;
						case TinyImageLayout::LAYOUT_DEPTHSTENCIL_ATTACHMENT:
							srcStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
							dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
							srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
							dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
						break;
						case TinyImageLayout::LAYOUT_GENERAL:
							srcStage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
							dstStage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
							srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
							dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
						break;
						case TinyImageLayout::LAYOUT_UNDEFINED:
						default:
							srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
							dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
							srcAccessMask = VK_ACCESS_NONE;
							dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;;
						break;
					}
				} else if (cmdBufferStage == TinyCmdBufferSubmitStage::STAGE_END) {
					switch (layout) {
						case TinyImageLayout::LAYOUT_COLOR_ATTACHMENT:
							srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
							dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
							srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
							dstAccessMask = VK_ACCESS_NONE;
						break;
						case TinyImageLayout::LAYOUT_PRESENT_SRC:
							srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
							dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
							srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
							dstAccessMask = VK_ACCESS_NONE;
						break;
						case TinyImageLayout::LAYOUT_TRANSFER_SRC:
							srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
							dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
							srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
							dstAccessMask = VK_ACCESS_NONE;
						break;
						case TinyImageLayout::LAYOUT_TRANSFER_DST:
							srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
							dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
							srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
							dstAccessMask = VK_ACCESS_NONE;
						break;
						case TinyImageLayout::LAYOUT_SHADER_READONLY:
							srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
							dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
							srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
							dstAccessMask = VK_ACCESS_NONE;
						break;
						case TinyImageLayout::LAYOUT_DEPTHSTENCIL_ATTACHMENT:
							srcStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
							dstStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
							srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
							dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
						break;
						case TinyImageLayout::LAYOUT_GENERAL:
							srcStage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
							dstStage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
							srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
							dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
						break;
						case TinyImageLayout::LAYOUT_UNDEFINED:
						default:
							srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
							dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
							srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;;
							dstAccessMask = VK_ACCESS_NONE;
						break;
					}
				} else if (cmdBufferStage == TinyCmdBufferSubmitStage::STAGE_BEGIN_TO_END) {
					srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
					dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
					srcAccessMask = VK_ACCESS_NONE;
					dstAccessMask = VK_ACCESS_NONE;
				}
			}
					
			/// @brief Get the pipeline barrier info for resource synchronization in image pipeline barrier pNext chain.
			VkImageMemoryBarrier GetPipelineBarrier(TinyImageLayout newLayout, TinyCmdBufferSubmitStage cmdBufferStage, VkPipelineStageFlags& srcStage, VkPipelineStageFlags& dstStage) {
				VkImageMemoryBarrier pipelineBarrier = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.oldLayout = (VkImageLayout) imageLayout, .newLayout = (VkImageLayout) newLayout,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1, },
					.image = image,
				};

				if (imageLayout == TinyImageLayout::LAYOUT_DEPTHSTENCIL_ATTACHMENT || newLayout == TinyImageLayout::LAYOUT_DEPTHSTENCIL_ATTACHMENT) {
					pipelineBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
					if (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT)
						pipelineBarrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
				}

				VkAccessFlags srcAccessMask, dstAccessMask;
				GetPipelineBarrierStages(newLayout, cmdBufferStage, srcStage, dstStage, srcAccessMask, dstAccessMask);
				pipelineBarrier.srcAccessMask = srcAccessMask;
				pipelineBarrier.dstAccessMask = dstAccessMask;
				return pipelineBarrier;
			}
			
			/// @brief Begins a transfer command and returns the command buffer index pair used for the command allocated from a TinyVkCommandPool.
			std::pair<VkCommandBuffer, int32_t> BeginTransferCmd() {
				std::pair<VkCommandBuffer, int32_t> bufferIndexPair = renderContext.commandPool.LeaseBuffer(true);

				VkCommandBufferBeginInfo beginInfo{};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
				vkBeginCommandBuffer(bufferIndexPair.first, &beginInfo);
				return bufferIndexPair;
			}

			/// @brief Ends a transfer command and gives the leased/rented command buffer pair back to the TinyVkCommandPool.
			void EndTransferCmd(std::pair<VkCommandBuffer, int32_t> bufferIndexPair) {
				vkEndCommandBuffer(bufferIndexPair.first);

				VkSubmitInfo submitInfo{};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.commandBufferCount = 1;
				submitInfo.pCommandBuffers = &bufferIndexPair.first;

				vkQueueSubmit(renderContext.graphicsPipeline.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
				vkQueueWaitIdle(renderContext.graphicsPipeline.graphicsQueue);
				vkResetCommandBuffer(bufferIndexPair.first, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
				renderContext.commandPool.ReturnBuffer(bufferIndexPair);
			}
			
			/// @brief Transitions the GPU bound VkImage from its current layout into a new layout.
			void TransitionLayoutCmd(TinyImageLayout newLayout) {
				std::pair<VkCommandBuffer, int32_t> bufferIndexPair = BeginTransferCmd();
				
				VkPipelineStageFlags srcStage, dstStage;
				VkImageMemoryBarrier pipelineBarrier = GetPipelineBarrier(newLayout, TinyCmdBufferSubmitStage::STAGE_BEGIN_TO_END, srcStage, dstStage);
				imageLayout = newLayout;
				aspectFlags = pipelineBarrier.subresourceRange.aspectMask;
				vkCmdPipelineBarrier(bufferIndexPair.first, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &pipelineBarrier);

				EndTransferCmd(bufferIndexPair);
			}

			/// @brief Transitions the GPU bound VkImage from its current layout into a new layout.
			void TransitionLayoutBarrier(VkCommandBuffer cmdBuffer, TinyCmdBufferSubmitStage cmdBufferStage, TinyImageLayout newLayout) {
				VkPipelineStageFlags srcStage, dstStage;
				VkImageMemoryBarrier pipelineBarrier = GetPipelineBarrier(newLayout, cmdBufferStage, srcStage, dstStage);
				imageLayout = newLayout;
				aspectFlags = pipelineBarrier.subresourceRange.aspectMask;
				vkCmdPipelineBarrier(cmdBuffer, srcStage, dstStage, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &pipelineBarrier);
			}

			/// @brief Copies data from CPU accessible memory to GPU accessible memory.
			void StageImageData(void* data, VkDeviceSize dataSize) {
				TinyObject<TinyBuffer> stagingBuffer = TinyBuffer::Construct(renderContext, TinyBufferType::TYPE_STAGING, dataSize);
				memcpy(stagingBuffer.ref().description.pMappedData, data, (size_t)dataSize);
				TransitionLayoutCmd(TinyImageLayout::LAYOUT_TRANSFER_DST);
				TransferFromBufferCmd(stagingBuffer);
				TransitionLayoutCmd(TinyImageLayout::LAYOUT_COLOR_ATTACHMENT);
			}

			/// @brief Copies data from the source TinyBuffer into this TinyImage (size = 0,0 uses this image size by default).
			void TransferFromBufferCmd(TinyBuffer& srcBuffer, VkExtent2D size = {0, 0}, VkOffset2D offset = {0, 0}) {
				std::pair<VkCommandBuffer, int32_t> bufferIndexPair = BeginTransferCmd();

				TransitionLayoutBarrier(bufferIndexPair.first, TinyCmdBufferSubmitStage::STAGE_BEGIN_TO_END, imageLayout);
				VkBufferImageCopy region = {
					.imageSubresource.aspectMask = aspectFlags, .bufferOffset = 0, .bufferRowLength = 0, .bufferImageHeight = 0,
					.imageSubresource.mipLevel = 0, .imageSubresource.baseArrayLayer = 0, .imageSubresource.layerCount = 1,
					.imageExtent = { static_cast<uint32_t>((size.width == 0)?width:size.width), static_cast<uint32_t>((size.height == 0)?height:size.height), 1 },
					.imageOffset = { static_cast<int32_t>(offset.x), static_cast<int32_t>(offset.y), 0 }
				};
				vkCmdCopyBufferToImage(bufferIndexPair.first, srcBuffer.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

				EndTransferCmd(bufferIndexPair);
			}
			
			/// @brief Copies data from this TinyImage into the destination TinyBuffer (size = 0,0 uses this image size by default).
			void TransferToBufferCmd(TinyBuffer& dstBuffer, VkExtent2D size = {0, 0}, VkOffset2D offset = {0, 0}) {
				std::pair<VkCommandBuffer, int32_t> bufferIndexPair = BeginTransferCmd();

				TransitionLayoutBarrier(bufferIndexPair.first, TinyCmdBufferSubmitStage::STAGE_BEGIN_TO_END, imageLayout);
				VkBufferImageCopy region = {
					.imageSubresource.aspectMask = aspectFlags, .bufferOffset = 0, .bufferRowLength = 0, .bufferImageHeight = 0,
					.imageSubresource.mipLevel = 0, .imageSubresource.baseArrayLayer = 0, .imageSubresource.layerCount = 1,
					.imageExtent = { static_cast<uint32_t>((size.width == 0)?width:size.width), static_cast<uint32_t>((size.height == 0)?height:size.height), 1 },
					.imageOffset = { static_cast<int32_t>(offset.x), static_cast<int32_t>(offset.y), 0 }
				};
				vkCmdCopyImageToBuffer(bufferIndexPair.first, image, (VkImageLayout) imageLayout, dstBuffer.buffer, 1, &region);

				EndTransferCmd(bufferIndexPair);
			}

			/// @brief Copies data from this TinyImage into the destination TinyBuffer (size = 0,0 uses this image size by default).
			inline static VkResult TransferImageCmd(TinyRenderContext& renderContext, TinyImage& srcImage, TinyImage& dstImage, VkDeviceSize dataSize, VkExtent2D size = {0, 0}, VkOffset2D srcOffset = {0, 0}, VkOffset2D dstOffset = {0, 0}) {
				if (srcImage.format != dstImage.format)
					return VK_ERROR_FORMAT_NOT_SUPPORTED;
				
				TinyBuffer buffer(renderContext, TinyBufferType::TYPE_STAGING, dataSize);
				srcImage.TransferToBufferCmd(buffer, size, srcOffset);
				dstImage.TransferFromBufferCmd(buffer, size, dstOffset);
                return VK_SUCCESS;
			}
			
			/// @brief Creates the data descriptor that represents this image when passing into graphicspipeline.SelectWrite*Descriptor().
			VkDescriptorImageInfo GetImageDescriptor() { return { imageSampler, imageView, (VkImageLayout) imageLayout }; }
			
			/// @brief Creates the image or assigns a swapchain image if passed.
			VkResult Initialize() {
				if (imageType == TinyImageType::TYPE_SWAPCHAIN) {
					if (image == VK_NULL_HANDLE)
						return VK_ERROR_INITIALIZATION_FAILED;
					imageLayout = TinyImageLayout::LAYOUT_UNDEFINED;
					aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
                    return VK_SUCCESS;
				} else {
					return ReCreateImage(imageType, width, height, format, addressingMode);
				}
            }

			/// @brief Constructor(...) + Initialize() with error result as combined TinyObject<Object,VkResult>.
			template<typename... A>
			inline static TinyObject<TinyImage> Construct(TinyRenderContext& renderContext, TinyImageType type, VkDeviceSize width, VkDeviceSize height, VkFormat format = VK_FORMAT_B8G8R8A8_UNORM, VkSamplerAddressMode addressingMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, bool textureInterpolation = false, VkImage imageSource = VK_NULL_HANDLE, VkImageView imageViewSource = VK_NULL_HANDLE, VkSampler imageSampler = VK_NULL_HANDLE, VkSemaphore imageAvailable = VK_NULL_HANDLE, VkSemaphore imageFinished = VK_NULL_HANDLE, VkFence imageWaitable = VK_NULL_HANDLE) {
				std::unique_ptr<TinyImage> object =
					std::make_unique<TinyImage>(renderContext, type, width, height, format, addressingMode, textureInterpolation, imageSource, imageViewSource, imageSampler, imageAvailable, imageFinished, imageWaitable);
				return TinyObject<TinyImage>(object, object->Initialize());
			}
		};
	}
#endif

///
///	ABOUT BUFFERS & IMAGES:
///		When Creating Buffers:
///			Buffers must be initialized with a VkDeviceSize, which is the size of the data in BYTES, not the
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