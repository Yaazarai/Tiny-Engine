#ifndef TINY_ENGINE_TINYIMAGE
#define TINY_ENGINE_TINYIMAGE
	#include "./TinyEngine.hpp"

	namespace TINY_ENGINE_NAMESPACE {
		/// @brief GPU device image for sending images to the render (GPU) device.
		class TinyImage : public TinyDisposable {
		public:
			TinyVkDevice& vkdevice;
			VmaAllocation memory = VK_NULL_HANDLE;
			VkImage image = VK_NULL_HANDLE;
			VkImageView imageView = VK_NULL_HANDLE;
			VkSampler imageSampler = VK_NULL_HANDLE;

			const TinyImageType imageType;
            VkDeviceSize width, height;
            bool interpolation;
            VkFormat imageFormat;
			TinyImageLayout imageLayout;
			VkImageAspectFlags aspectFlags;
			VkSamplerAddressMode addressMode;
			VkResult initialized = VK_ERROR_INITIALIZATION_FAILED;
			
			TinyImage operator=(const TinyImage&) = delete;
			TinyImage(const TinyImage&) = delete;
			~TinyImage() { this->Dispose(); }

			void Disposable(bool waitIdle) {
				if (waitIdle) vkDeviceWaitIdle(vkdevice.logicalDevice);
				if (imageType != TinyImageType::TYPE_SWAPCHAIN) {
					if (imageSampler != VK_NULL_HANDLE) vkDestroySampler(vkdevice.logicalDevice, imageSampler, VK_NULL_HANDLE);
					if (imageView != VK_NULL_HANDLE) vkDestroyImageView(vkdevice.logicalDevice, imageView, VK_NULL_HANDLE);
					if (image != VK_NULL_HANDLE) vmaDestroyImage(vkdevice.memoryAllocator, image, memory);
				}
			}

            TinyImage(TinyVkDevice& vkdevice, const TinyImageType imageType, VkDeviceSize width, VkDeviceSize height, VkFormat imageFormat = VK_FORMAT_B8G8R8A8_UNORM, VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, bool interpolation = false, VkImage imageSource = VK_NULL_HANDLE, VkImageView imageViewSource = VK_NULL_HANDLE, VkSampler imageSampler = VK_NULL_HANDLE)
            : vkdevice(vkdevice), imageType(imageType), width(width), height(height), imageFormat(imageFormat), addressMode(addressMode), interpolation(interpolation), image(imageSource), imageView(imageViewSource), imageSampler(imageSampler) {
                onDispose.hook(TinyCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));
				initialized = Initialize();
            }

            VkResult CreateImage(TinyImageType type, VkDeviceSize width, VkDeviceSize height, VkFormat format = VK_FORMAT_R16G16B16A16_UNORM, VkSamplerAddressMode addressingMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, bool textureInterpolation = false) {
				if (type == TinyImageType::TYPE_SWAPCHAIN) return VK_ERROR_INITIALIZATION_FAILED;

				VkImageCreateInfo imgCreateInfo = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.extent.width = static_cast<uint32_t>(width), .extent.height = static_cast<uint32_t>(height),
					.extent.depth = 1, .mipLevels = 1, .arrayLayers = 1,
					.format = format, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, .imageType = VK_IMAGE_TYPE_2D,
					.tiling = VK_IMAGE_TILING_OPTIMAL, .samples = VK_SAMPLE_COUNT_1_BIT,
					.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
				};
				
				this->width = width;
				this->height = height;
				this->imageLayout = TinyImageLayout::LAYOUT_UNDEFINED;
				this->aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
				this->interpolation = interpolation;

				VmaAllocationCreateInfo allocCreateInfo {};
				allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
				allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
				allocCreateInfo.priority = 1.0f;
				
				VkResult result = vmaCreateImage(vkdevice.memoryAllocator, &imgCreateInfo, &allocCreateInfo, &image, &memory, VK_NULL_HANDLE);
				if (result != VK_SUCCESS) return result;
				
				VkPhysicalDeviceProperties properties {};
				vkGetPhysicalDeviceProperties(vkdevice.physicalDevice, &properties);

				VkFilter filter = (interpolation == true)? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
				VkSamplerMipmapMode mipmapMode = (interpolation)? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
				float interpolationWeight = (interpolation)? VK_LOD_CLAMP_NONE : 0.0f;

				addressMode = addressingMode;
				VkSamplerCreateInfo samplerInfo {
					.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
					.magFilter = filter, .minFilter = filter,
					.anisotropyEnable = VK_FALSE, .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
					.addressModeU = addressMode, .addressModeV = addressMode, .addressModeW = addressMode, .unnormalizedCoordinates = VK_FALSE,
					.compareEnable = VK_FALSE, .compareOp = VK_COMPARE_OP_ALWAYS,
					.mipmapMode = mipmapMode, .mipLodBias = 0.0f, .minLod = 0.0f, .maxLod = interpolationWeight,
					.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
				};

				result = vkCreateSampler(vkdevice.logicalDevice, &samplerInfo, VK_NULL_HANDLE, &imageSampler);
				if (result != VK_SUCCESS) return result;

				VkImageViewCreateInfo createInfo {
					.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
					.image = image, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = imageFormat, .components = { VK_COMPONENT_SWIZZLE_IDENTITY },
					.subresourceRange = { .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1, .aspectMask = aspectFlags, }
				};

				return vkCreateImageView(vkdevice.logicalDevice, &createInfo, VK_NULL_HANDLE, &imageView);
			}
			
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
							dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
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
			
			VkImageMemoryBarrier GetPipelineBarrier(TinyImageLayout newLayout, TinyCmdBufferSubmitStage cmdBufferStage, VkPipelineStageFlags& srcStage, VkPipelineStageFlags& dstStage) {
				VkImageMemoryBarrier pipelineBarrier = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.oldLayout = (VkImageLayout) imageLayout, .newLayout = (VkImageLayout) newLayout,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1, },
					.image = image,
				};

				VkAccessFlags srcAccessMask, dstAccessMask;
				GetPipelineBarrierStages(newLayout, cmdBufferStage, srcStage, dstStage, srcAccessMask, dstAccessMask);
				pipelineBarrier.srcAccessMask = srcAccessMask;
				pipelineBarrier.dstAccessMask = dstAccessMask;
				return pipelineBarrier;
			}

			void TransitionLayoutBarrier(VkCommandBuffer cmdBuffer, TinyCmdBufferSubmitStage cmdBufferStage, TinyImageLayout newLayout) {
				VkPipelineStageFlags srcStage, dstStage;
				VkImageMemoryBarrier pipelineBarrier = GetPipelineBarrier(newLayout, cmdBufferStage, srcStage, dstStage);
				imageLayout = newLayout;
				aspectFlags = pipelineBarrier.subresourceRange.aspectMask;
				vkCmdPipelineBarrier(cmdBuffer, srcStage, dstStage, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &pipelineBarrier);
			}
			
			VkDescriptorImageInfo GetDescriptorInfo() {
                return { imageSampler, imageView, (VkImageLayout) imageLayout };
            }
            
			inline static VkWriteDescriptorSet GetWriteDescriptor(uint32_t binding, uint32_t descriptorCount, const VkDescriptorImageInfo* imageInfo) {
				return { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pImageInfo = imageInfo, .dstSet = 0, .dstBinding = binding, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = descriptorCount };
			}
			
			VkResult Initialize() {
				if (imageType == TinyImageType::TYPE_SWAPCHAIN) {
					imageLayout = TinyImageLayout::LAYOUT_UNDEFINED;
					aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
					imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
					
					if (image == VK_NULL_HANDLE)
						return VK_ERROR_INITIALIZATION_FAILED;
                    return VK_SUCCESS;
				} else {
					return CreateImage(imageType, width, height, imageFormat, addressMode);
				}
            }
		};
	}
#endif