#ifndef TINY_ENGINE_RENDERGRAPH
#define TINY_ENGINE_RENDERGRAPH
	#include "./TinyEngine.hpp"

	namespace TINY_ENGINE_NAMESPACE {
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

			std::vector<TinyRenderPass*> CreateRenderPass(TinyCommandPool& cmdPool, TinyPipeline& pipeline, TinyImage* targetImage, std::string title, VkDeviceSize subpassCount = 1, uint32_t maxTimestamps = 16U) {
				std::vector<TinyRenderPass*> subpasses;
				for(int32_t i = 0; i < std::max(1, static_cast<int32_t>(subpassCount)); i++) {
					TinyRenderPass* renderpass = new TinyRenderPass(vkdevice, cmdPool, pipeline, targetImage, title, renderPassCounter ++, i, maxTimestamps);
					
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
				}

				VkResult result = VK_SUCCESS;
				for(int32_t i = 0; i < renderPasses.size(); i++) {
					if (renderPasses[i]->pipeline.createInfo.type == TinyPipelineType::TYPE_PRESENT)
						renderPasses[i]->targetImage = swapChainImages[swapFrameIndex];

					////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
					std::pair<VkCommandBuffer, int32_t> cmdbufferPair = (renderPasses[i]->pipeline.createInfo.type == TinyPipelineType::TYPE_TRANSFER)?
						renderPasses[i]->BeginStageCmdBuffer() : renderPasses[i]->BeginRecordCmdBuffer();
					
					TinyRenderObject executionObject(renderPasses[i]->pipeline, cmdbufferPair);
					renderPasses[i]->renderEvent.invoke(*renderPasses[i], executionObject, static_cast<bool>(frameResized));
					
					if (renderPasses[i]->pipeline.createInfo.type == TinyPipelineType::TYPE_TRANSFER) {
						renderPasses[i]->EndStageCmdBuffer(cmdbufferPair);
					} else { renderPasses[i]->EndRecordCmdBuffer(cmdbufferPair); }
					////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
					
					VkDeviceSize frameWait = frameCounter * 100;
					VkDeviceSize waitValue = frameWait + renderPasses[i]->timelineWait;
					VkDeviceSize signalValue = frameWait + renderPasses[i]->subpassIndex;
					VkSemaphore initialWaits[] = { swapImageAvailable };
					VkSemaphore dependencyWaits[] { swapImageTimeline };
					VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
					bool isInitialPass = i == 0 || renderPasses[i]->dependencies.size() == 0;

					VkTimelineSemaphoreSubmitInfo timelineInfo = { .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
						.waitSemaphoreValueCount = 1, .pWaitSemaphoreValues = &waitValue, .signalSemaphoreValueCount = 1, .pSignalSemaphoreValues = &signalValue };
					
					VkSubmitInfo submitInfo { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1U, .pCommandBuffers = &cmdbufferPair.first,
						.pWaitDstStageMask = waitStages, .waitSemaphoreCount = static_cast<uint32_t>(isInitialPass), .pWaitSemaphores = (isInitialPass)? initialWaits : dependencyWaits, .pNext = &timelineInfo };
					
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