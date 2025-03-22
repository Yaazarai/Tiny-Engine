#ifndef TINY_ENGINE_RENDERGRAPH
#define TINY_ENGINE_RENDERGRAPH
	#include "./TinyEngine.hpp"

	namespace TINY_ENGINE_NAMESPACE {
        class TinyRenderPass : public TinyDisposable {
        public:
			TinyVkDevice& vkdevice;
			TinyCommandPool& cmdPool;
			TinyPipeline& pipeline;

            TinyImage* targetImage;
			const VkBool32 pushToPresent;
			const VkDeviceSize subpassIndex;
			std::vector<TinyRenderPass*> dependencies;
			TinyInvokable<TinyRenderPass&, TinyCommandPool&, std::vector<VkCommandBuffer>&> onRender;
			VkSemaphore renderPassFinished;
			VkFence renderPassSignal;
			VkDeviceSize timelineWait;

			TinyRenderPass operator=(const TinyRenderPass&) = delete;
			TinyRenderPass(const TinyRenderPass&) = delete;
			~TinyRenderPass() { this->Dispose(); }
            void Disposable(bool waitIdle) {
				if (waitIdle) vkdevice.DeviceWaitIdle();
				delete targetImage;
			}

			TinyRenderPass(TinyVkDevice& vkdevice, TinyCommandPool& cmdPool, TinyPipeline& pipeline, VkDeviceSize subpassIndex, VkExtent2D subpassExtent, const VkBool32 pushToPresent = VK_FALSE)
			: vkdevice(vkdevice), cmdPool(cmdPool), pipeline(pipeline), subpassIndex(subpassIndex), pushToPresent(pushToPresent), timelineWait(0) {
				if (pushToPresent == VK_FALSE) {
					targetImage = new TinyImage(vkdevice, TinyImageType::TYPE_COLORATTACHMENT, subpassExtent.width, subpassExtent.height, pipeline.createInfo.imageFormat, pipeline.createInfo.addressMode, pipeline.createInfo.interpolation);
					targetImage->Initialize();
				}
				onDispose.hook(TinyCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));
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

			void PushConstants(VkCommandBuffer cmdBuffer, TinyShaderStages shaderFlags, uint32_t byteSize, const void* pValues) {
				vkCmdPushConstants(cmdBuffer, pipeline.layout, (VkShaderStageFlagBits) shaderFlags, 0, byteSize, pValues);
			}

			void PushDescriptorSet(VkCommandBuffer cmdBuffer, std::vector<VkWriteDescriptorSet> writeDescriptorSets) {
				vkCmdPushDescriptorSetEKHR(pipeline.vkdevice.instance, cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, 0, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data());
			}
			
			VkResult BeginRecordCmdBuffer(VkCommandBuffer targetCmdBuffer, const VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f }, const VkClearValue depthStencil = { .depthStencil = { 1.0f, 0 } }) {
				VkCommandBufferBeginInfo beginInfo {};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

				VkResult result = vkBeginCommandBuffer(targetCmdBuffer, &beginInfo);
				if (result != VK_SUCCESS) return result;
                
                targetImage->TransitionLayoutBarrier(targetCmdBuffer, TinyCmdBufferSubmitStage::STAGE_BEGIN, TinyImageLayout::LAYOUT_COLOR_ATTACHMENT);

				VkViewport dynamicViewportKHR { .x = 0, .y = 0, .minDepth = 0.0f, .maxDepth = 1.0f, .width = static_cast<float>(targetImage->width), .height = static_cast<float>(targetImage->height) };
				vkCmdSetViewport(targetCmdBuffer, 0, 1, &dynamicViewportKHR);
				
				VkRect2D renderAreaKHR { .extent = { .width = static_cast<uint32_t>(targetImage->width), .height = static_cast<uint32_t>(targetImage->height) } };
				vkCmdSetScissor(targetCmdBuffer, 0, 1, &renderAreaKHR);
                
				VkRenderingAttachmentInfoKHR colorAttachmentInfo { .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
					.clearValue = clearColor, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.imageView = targetImage->imageView, .imageLayout = (VkImageLayout) targetImage->imageLayout
				};
				VkRenderingInfoKHR dynamicRenderInfo { .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR, .colorAttachmentCount = 1, .pColorAttachments = &colorAttachmentInfo, .renderArea = renderAreaKHR, .layerCount = 1 };
				result = vkCmdBeginRenderingEKHR(pipeline.vkdevice.instance, targetCmdBuffer, &dynamicRenderInfo);
				if (result != VK_SUCCESS) return result;
				
				vkCmdBindPipeline(targetCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
                return VK_SUCCESS;
			}
			
			VkResult EndRecordCmdBuffer(VkCommandBuffer targetCmdBuffer, const VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f }, const VkClearValue depthStencil = { .depthStencil = { 1.0f, 0 } }) {
				VkResult result = vkCmdEndRenderingEKHR(pipeline.vkdevice.instance, targetCmdBuffer);
				if (result != VK_SUCCESS) return result;
                
                if (targetImage->imageType == TinyImageType::TYPE_SWAPCHAIN) {
					targetImage->TransitionLayoutBarrier(targetCmdBuffer, TinyCmdBufferSubmitStage::STAGE_END, TinyImageLayout::LAYOUT_PRESENT_SRC);
				} else {
					targetImage->TransitionLayoutBarrier(targetCmdBuffer, TinyCmdBufferSubmitStage::STAGE_END, TinyImageLayout::LAYOUT_SHADER_READONLY);
				}

				return vkEndCommandBuffer(targetCmdBuffer);
			}
			
			template<typename... A>
			inline static TinyObject<TinyRenderPass> Construct(TinyVkDevice& vkdevice, TinyCommandPool& cmdPool, TinyPipeline& pipeline, VkDeviceSize subpassIndex, VkExtent2D subpassExtent, const VkBool32 pushToPresent = VK_FALSE) {
				std::unique_ptr<TinyRenderPass> object =
					std::make_unique<TinyRenderPass>(vkdevice, cmdPool, pipeline, subpassIndex, subpassExtent, pushToPresent);
				return TinyObject<TinyRenderPass>(object, VK_SUCCESS);
			}
        };

        class TinyRenderGraph : public TinyDisposable {
		public:
			TinyVkDevice& vkdevice;
			TinyWindow* window;

			std::atomic_int64_t frameCounter, renderPassCounter;
			VkFence swapImageInFlight;
			VkSemaphore swapImageAvailable;
			VkSemaphore swapImageFinished;
			VkSemaphore swapImageTimeline;
            
			std::timed_mutex swapChainMutex;
			VkSwapchainKHR swapChain;
			TinySurfaceSupporter swapChainPresentDetails;
			VkQueue swapChainPresentQueue;
			
			std::vector<TinyImage*> swapChainImages;
			std::atomic_bool presentable, refreshable;
			uint32_t swapFrameIndex;

			std::vector<TinyRenderPass*> renderPasses;
			inline static TinyInvokable<> onResizeFrameBuffer;

			TinyRenderGraph operator=(const TinyRenderGraph&) = delete;
			TinyRenderGraph(const TinyRenderGraph&) = delete;
			~TinyRenderGraph() { this->Dispose(); }

			void Disposable(bool waitIdle) {
				if (waitIdle) vkdevice.DeviceWaitIdle();

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

			TinyRenderGraph(TinyVkDevice& vkdevice, TinyWindow* window) : vkdevice(vkdevice), window(window), presentable(true), refreshable(false), swapChain(VK_NULL_HANDLE), renderPassCounter(0), frameCounter(0), swapFrameIndex(0) {
				onDispose.hook(TinyCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));
			}
			
			std::vector<TinyRenderPass*> CreateRenderPass(TinyCommandPool& cmdPool, TinyPipeline& pipeline, VkExtent2D extent, VkDeviceSize subpassCount = 1, VkBool32 pushToPresent = VK_FALSE) {
				std::vector<TinyRenderPass*> subpasses;
				for(int32_t i = 0; i < std::max(1, static_cast<int32_t>(subpassCount)); i++) {
					TinyRenderPass* renderpass = new TinyRenderPass(vkdevice, cmdPool, pipeline, renderPassCounter ++, extent, (i == subpassCount - 1)? pushToPresent : VK_FALSE);
					renderPasses.push_back(renderpass);
					subpasses.push_back(renderpass);
                    
					if (i > 0)
						subpasses[i]->AddDependency(*subpasses[i - 1]);
				}
				return subpasses;
			}
			
			void ResizeFrameBuffer(GLFWwindow* hwndWindow, int width, int height) {
				if (width == 0 || height == 0) return;

				for(TinyImage* swapImage : swapChainImages) {
					vkDestroyImageView(vkdevice.logicalDevice, swapImage->imageView, VK_NULL_HANDLE);
					delete swapImage;
				}
				
				VkSwapchainKHR oldSwapChain = swapChain;
				TinySwapchain::CreateSwapChainImages(vkdevice, *window, swapChainPresentDetails, swapChain, swapChainImages);
				TinySwapchain::CreateSwapChainImageViews(vkdevice, swapChainPresentDetails, swapChainImages);
				vkDestroySwapchainKHR(vkdevice.logicalDevice, oldSwapChain, VK_NULL_HANDLE);

				presentable = true;
				refreshable = false;
			}
			
			VkResult ExecuteRenderGraph() {
				for(int32_t i = 0; i < renderPasses.size(); i++)
					renderPasses[i]->cmdPool.ReturnAllBuffers();
				
				VkResult result = VK_SUCCESS;
				for(int32_t i = 0; i < renderPasses.size(); i++) {
					if (renderPasses[i]->pushToPresent == VK_TRUE)
						renderPasses[i]->targetImage = swapChainImages[swapFrameIndex];
					
					std::vector<VkCommandBuffer> cmdbuffers;
					renderPasses[i]->onRender.invoke(*renderPasses[i], renderPasses[i]->cmdPool, cmdbuffers);

					VkSubmitInfo submitInfo{ .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = static_cast<uint32_t>(cmdbuffers.size()), .pCommandBuffers = cmdbuffers.data() };
					VkTimelineSemaphoreSubmitInfo timelineInfo = { .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO, .waitSemaphoreValueCount = 0, .pWaitSemaphoreValues = VK_NULL_HANDLE, .pNext = VK_NULL_HANDLE };

					VkDeviceSize frameWait = frameCounter * 100;
					VkDeviceSize waitValue = frameWait + renderPasses[i]->timelineWait;
					VkDeviceSize signalValue = frameWait + renderPasses[i]->subpassIndex;
					VkSemaphore initialWaits[] = { swapImageAvailable };
					VkSemaphore dependencyWaits[] { swapImageTimeline };
					
					VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
					submitInfo.waitSemaphoreCount = 1;
					submitInfo.pWaitDstStageMask = waitStages;
					submitInfo.pWaitSemaphores = (i == 0 || renderPasses[i]->dependencies.size() == 0)? initialWaits : dependencyWaits;
					
					timelineInfo.waitSemaphoreValueCount = 1;
					timelineInfo.pWaitSemaphoreValues = &waitValue;
					timelineInfo.signalSemaphoreValueCount = 1;
					timelineInfo.pSignalSemaphoreValues = &signalValue;
					submitInfo.pNext = &timelineInfo;
					
					if (renderPasses[i]->targetImage->imageType == TinyImageType::TYPE_SWAPCHAIN) {
						VkSemaphore signalSemaphores[] = {  swapImageFinished };
						submitInfo.signalSemaphoreCount = 1;
						submitInfo.pSignalSemaphores = signalSemaphores;
						result = vkQueueSubmit(swapChainPresentQueue, 1, &submitInfo, swapImageInFlight);
					} else {
						vkQueueSubmit(renderPasses[i]->pipeline.submitQueue, 1, &submitInfo, VK_NULL_HANDLE);
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

					if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
						ExecuteRenderGraph();
						result = TinySwapchain::QueuePresent(swapChainPresentQueue, swapChain, swapImageFinished, swapFrameIndex);
					}

					presentable = (result == VK_SUCCESS);
					frameCounter ++;
				}
				return result;
			}
			
			VkResult Initialize() {
				if (window != VK_NULL_HANDLE) {
					TinyQueueFamily indices = vkdevice.QueryPhysicalDeviceQueueFamilies();
					if (!indices.hasPresentFamily) return VK_ERROR_INITIALIZATION_FAILED;
					vkGetDeviceQueue(vkdevice.logicalDevice, indices.presentFamily, 0, &swapChainPresentQueue);

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

			template<typename... A>
			inline static TinyObject<TinyRenderGraph> Construct(TinyVkDevice& vkdevice, TinyWindow* window) {
				std::unique_ptr<TinyRenderGraph> object =
					std::make_unique<TinyRenderGraph>(vkdevice, window);
				return TinyObject<TinyRenderGraph>(object, object->Initialize());
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