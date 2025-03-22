#ifndef TINY_ENGINE_TINYRENDERCMD
#define TINY_ENGINE_TINYRENDERCMD
	#include "./TinyEngine.hpp"
	#include "vulkan/vulkan_format_traits.hpp"

	namespace TINY_ENGINE_NAMESPACE {
		class TinySingleSubmitCmds {
		public:
			static std::pair<VkCommandBuffer, int32_t> StartCmd(TinyCommandPool&  cmdpool) {
				std::pair<VkCommandBuffer, int32_t> bufferIndexPair = cmdpool.LeaseBuffer(true);
				VkCommandBufferBeginInfo beginInfo { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT	 };
				vkBeginCommandBuffer(bufferIndexPair.first, &beginInfo);
				return bufferIndexPair;
			}

			static void SubmitCmd(TinyPipeline& pipeline, TinyCommandPool& cmdpool, std::pair<VkCommandBuffer, int32_t> bufferIndexPair) {
				vkEndCommandBuffer(bufferIndexPair.first);
				
				VkSubmitInfo submitInfo { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .pCommandBuffers = &bufferIndexPair.first, .commandBufferCount = 1 };
				vkQueueWaitIdle(pipeline.submitQueue);
				vkQueueSubmit(pipeline.submitQueue, 1, &submitInfo, VK_NULL_HANDLE);
				vkQueueWaitIdle(pipeline.submitQueue);
				
				vkResetCommandBuffer(bufferIndexPair.first, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
				cmdpool.ReturnBuffer(bufferIndexPair);
			}

			static void StageBufferData(TinyBuffer& destBuffer, TinyPipeline& pipeline, TinyCommandPool&  cmdpool, void* data, VkDeviceSize dataSize, VkDeviceSize srceOffset = 0, VkDeviceSize destOffset = 0) {
				std::pair<VkCommandBuffer,int32_t> bufferIndexPair = StartCmd(cmdpool);

				TinyObject<TinyBuffer> stagingBuffer = TinyBuffer::Construct(destBuffer.vkdevice, TinyBufferType::TYPE_STAGING, dataSize);
				memcpy(stagingBuffer.ref().description.pMappedData, data, (size_t)dataSize);

				VkBufferCopy copyRegion { .srcOffset = srceOffset, .dstOffset = destOffset, .size = dataSize };
				vkCmdCopyBuffer(bufferIndexPair.first, stagingBuffer.ref().buffer, destBuffer.buffer, 1, &copyRegion);

				SubmitCmd(pipeline, cmdpool, bufferIndexPair);
			}

			static void StageImageData(TinyImage& destImage, TinyPipeline& pipeline, TinyCommandPool& cmdpool, void* data, VkDeviceSize dataSize, VkExtent2D size = {0, 0}, VkOffset2D offset = {0, 0}) {
				std::pair<VkCommandBuffer,int32_t> bufferIndexPair = StartCmd(cmdpool);

				TinyObject<TinyBuffer> stagingBuffer = TinyBuffer::Construct(destImage.vkdevice, TinyBufferType::TYPE_STAGING, dataSize);
				memcpy(stagingBuffer.ref().description.pMappedData, data, (size_t)dataSize);

				TinyImageLayout prevLayout = destImage.imageLayout;
				destImage.TransitionLayoutBarrier(bufferIndexPair.first, TinyCmdBufferSubmitStage::STAGE_BEGIN, TinyImageLayout::LAYOUT_TRANSFER_DST);
				VkBufferImageCopy region = {
					.imageSubresource.aspectMask = destImage.aspectFlags, .bufferOffset = 0, .bufferRowLength = 0, .bufferImageHeight = 0,
					.imageSubresource.mipLevel = 0, .imageSubresource.baseArrayLayer = 0, .imageSubresource.layerCount = 1,
					.imageExtent = { static_cast<uint32_t>((size.width == 0)?destImage.width:size.width), static_cast<uint32_t>((size.height == 0)?destImage.height:size.height), 1 },
					.imageOffset = { static_cast<int32_t>(offset.x), static_cast<int32_t>(offset.y), 0 }
				};
				vkCmdCopyBufferToImage(bufferIndexPair.first, stagingBuffer.ref().buffer, destImage.image, (VkImageLayout) destImage.imageLayout, 1, &region);
				destImage.TransitionLayoutBarrier(bufferIndexPair.first, TinyCmdBufferSubmitStage::STAGE_END, prevLayout);

				SubmitCmd(pipeline, cmdpool, bufferIndexPair);
			}

			static void CopyImageToBuffer(TinyImage& srceImage, TinyBuffer& destBuffer, TinyPipeline& pipeline, TinyCommandPool& cmdpool, VkExtent2D size = {0, 0}, VkOffset2D offset = {0, 0}) {
				std::pair<VkCommandBuffer,int32_t> bufferIndexPair = StartCmd(cmdpool);

				TinyImageLayout prevLayout = srceImage.imageLayout;
				srceImage.TransitionLayoutBarrier(bufferIndexPair.first, TinyCmdBufferSubmitStage::STAGE_BEGIN, TinyImageLayout::LAYOUT_TRANSFER_SRC);
				VkBufferImageCopy region = {
					.imageSubresource.aspectMask = srceImage.aspectFlags, .bufferOffset = 0, .bufferRowLength = 0, .bufferImageHeight = 0,
					.imageSubresource.mipLevel = 0, .imageSubresource.baseArrayLayer = 0, .imageSubresource.layerCount = 1,
					.imageExtent = { static_cast<uint32_t>((size.width == 0)?srceImage.width:size.width), static_cast<uint32_t>((size.height == 0)?srceImage.height:size.height), 1 },
					.imageOffset = { static_cast<int32_t>(offset.x), static_cast<int32_t>(offset.y), 0 }
				};
				vkCmdCopyImageToBuffer(bufferIndexPair.first, srceImage.image, (VkImageLayout) srceImage.imageLayout, destBuffer.buffer, 1, &region);
				srceImage.TransitionLayoutBarrier(bufferIndexPair.first, TinyCmdBufferSubmitStage::STAGE_END, prevLayout);

				SubmitCmd(pipeline, cmdpool, bufferIndexPair);
			}

			static void CopyBufferToImage(TinyBuffer& srceBuffer, TinyImage& destImage, TinyPipeline& pipeline, TinyCommandPool& cmdpool, VkExtent2D size = {0, 0}, VkOffset2D offset = {0, 0}) {
				std::pair<VkCommandBuffer,int32_t> bufferIndexPair = StartCmd(cmdpool);

				TinyImageLayout prevLayout = destImage.imageLayout;
				destImage.TransitionLayoutBarrier(bufferIndexPair.first, TinyCmdBufferSubmitStage::STAGE_BEGIN, TinyImageLayout::LAYOUT_TRANSFER_DST);
				VkBufferImageCopy region = {
					.imageSubresource.aspectMask = destImage.aspectFlags, .bufferOffset = 0, .bufferRowLength = 0, .bufferImageHeight = 0,
					.imageSubresource.mipLevel = 0, .imageSubresource.baseArrayLayer = 0, .imageSubresource.layerCount = 1,
					.imageExtent = { static_cast<uint32_t>((size.width == 0)?destImage.width:size.width), static_cast<uint32_t>((size.height == 0)?destImage.height:size.height), 1 },
					.imageOffset = { static_cast<int32_t>(offset.x), static_cast<int32_t>(offset.y), 0 }
				};
				vkCmdCopyBufferToImage(bufferIndexPair.first, srceBuffer.buffer, destImage.image, (VkImageLayout) destImage.imageLayout, 1, &region);
				destImage.TransitionLayoutBarrier(bufferIndexPair.first, TinyCmdBufferSubmitStage::STAGE_END, prevLayout);

				SubmitCmd(pipeline, cmdpool, bufferIndexPair);
			}

			static void CopyBufferToBuffer(TinyBuffer& destBuffer, TinyBuffer& srceBuffer, TinyPipeline& pipeline, TinyCommandPool&  cmdpool, VkDeviceSize dataSize, VkDeviceSize srceOffset = 0, VkDeviceSize destOffset = 0) {
				std::pair<VkCommandBuffer,int32_t> bufferIndexPair = StartCmd(cmdpool);

				VkBufferCopy copyRegion { .srcOffset = srceOffset, .dstOffset = destOffset, .size = dataSize };
				vkCmdCopyBuffer(bufferIndexPair.first, srceBuffer.buffer, destBuffer.buffer, 1, &copyRegion);

				SubmitCmd(pipeline, cmdpool, bufferIndexPair);
			}

			static void CopyImageToImage(TinyImage& destImage, TinyImage& srceImage, TinyPipeline& pipeline, TinyCommandPool&  cmdpool, VkExtent2D size = {0, 0}, VkOffset2D destOffset = {0, 0}, VkOffset2D srceOffset = {0, 0}) {
				std::pair<VkCommandBuffer, int32_t> bufferIndexPair = StartCmd(cmdpool);

				TinyImageLayout srcePrevLayout = srceImage.imageLayout, 
					destPrevLayout = destImage.imageLayout;
				srceImage.TransitionLayoutBarrier(bufferIndexPair.first, TinyCmdBufferSubmitStage::STAGE_BEGIN, TinyImageLayout::LAYOUT_TRANSFER_SRC);
				destImage.TransitionLayoutBarrier(bufferIndexPair.first, TinyCmdBufferSubmitStage::STAGE_BEGIN, TinyImageLayout::LAYOUT_TRANSFER_DST);
				
				VkImageCopy copyRegion {
					.srcOffset = {srceOffset.x, srceOffset.y, 0}, .srcSubresource.aspectMask = destImage.aspectFlags, .srcSubresource.baseArrayLayer = 0, .srcSubresource.mipLevel = 0, .srcSubresource.layerCount = 1,
					.dstOffset = {destOffset.x, destOffset.y, 0}, .dstSubresource.aspectMask = destImage.aspectFlags, .dstSubresource.baseArrayLayer = 0, .dstSubresource.mipLevel = 0, .dstSubresource.layerCount = 1,
					.extent = { static_cast<uint32_t>((size.width == 0)?destImage.width:size.width), static_cast<uint32_t>((size.height == 0)?destImage.height:size.height), 1 }
				};
				vkCmdCopyImage(bufferIndexPair.first, srceImage.image, (VkImageLayout) srceImage.imageLayout, destImage.image, (VkImageLayout) destImage.imageLayout, 1, &copyRegion);
				
				srceImage.TransitionLayoutBarrier(bufferIndexPair.first, TinyCmdBufferSubmitStage::STAGE_END, srcePrevLayout);
				destImage.TransitionLayoutBarrier(bufferIndexPair.first, TinyCmdBufferSubmitStage::STAGE_END, destPrevLayout);

				SubmitCmd(pipeline, cmdpool, bufferIndexPair);
			}
		};
	}
#endif