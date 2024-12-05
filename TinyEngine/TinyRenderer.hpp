#pragma once
#ifndef TINY_ENGINE_RENDERER
#define TINY_ENGINE_RENDERER
	#include "./TinyEngine.hpp"

	namespace TINY_ENGINE_NAMESPACE {
		/// @brief Offscreen Rendering (Render-To-Texture Model): Render to VkImage.
		class TinyRenderer : public TinyRenderInterface {
		protected:
			TinyImage* optionalDepthImage;
			TinyImage* renderTarget;
			TinyCommandPool* commandPool;

		public:
			TinyRenderContext& renderContext;

            /// Invokable Render Events: (executed in TinyRenderer::RenderExecute()
			TinyInvokable<TinyCommandPool&> onRenderEvents;

			/// @brief Deletes the copy-constructor (dynamic resources cannot be copied).
			TinyRenderer operator=(const TinyRenderer&) = delete;
			
			/// @brief Deletes the copy-constructor (dynamic resources cannot be copied).
			TinyRenderer(const TinyRenderer&) = delete;

			/// @brief Calls the default destructor (nothing to dispose).
			~TinyRenderer() {}
            
            /// @brief Simple render-to-image graphics pipeline renderer.
			TinyRenderer(TinyRenderContext& renderContext, TinyCommandPool* cmdPool, TinyImage* renderTarget, TinyImage* optionalDepthImage = VK_NULL_HANDLE)
            : renderContext(renderContext), commandPool(cmdPool), renderTarget(renderTarget), optionalDepthImage(optionalDepthImage) {}

			/// @brief Sets the target image/texture for the TinyImageRenderer.
			VkResult SetRenderTarget(TinyCommandPool* cmdPool, TinyImage* renderTarget, TinyImage* optionalDepthImage = VK_NULL_HANDLE, bool waitOldTarget = true) {
				if (this->renderTarget != VK_NULL_HANDLE && waitOldTarget) {
					vkWaitForFences(renderContext.vkdevice.logicalDevice, 1, &renderTarget->imageWaitable, VK_TRUE, UINT64_MAX);
					vkResetFences(renderContext.vkdevice.logicalDevice, 1, &renderTarget->imageWaitable);
				}

                if (renderContext.graphicsPipeline.enableDepthTesting && optionalDepthImage == VK_NULL_HANDLE)
                    return VK_ERROR_INITIALIZATION_FAILED;
                
                this->commandPool = cmdPool;
				this->renderTarget = renderTarget;
                this->optionalDepthImage = optionalDepthImage;
				return VK_SUCCESS;
			}

			/// @brief Records Push Constants to the command buffer.
			void PushConstants(VkCommandBuffer cmdBuffer, TinyShaderStages shaderFlags, uint32_t byteSize, const void* pValues) {
				vkCmdPushConstants(cmdBuffer, renderContext.graphicsPipeline.pipelineLayout, (VkShaderStageFlagBits) shaderFlags, 0, byteSize, pValues);
			}

			/// @brief Records Push Descriptors to the command buffer.
			void PushDescriptorSet(VkCommandBuffer cmdBuffer, std::vector<VkWriteDescriptorSet> writeDescriptorSets) {
				vkCmdPushDescriptorSetEKHR(renderContext.vkdevice.instance, cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderContext.graphicsPipeline.pipelineLayout, 0, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data());
			}
            
			/// @brief Begins recording render commands to the provided command buffer.
			VkResult BeginRecordCmdBuffer(VkCommandBuffer commandBuffer, std::vector<TinyImage*> syncImages = {}, std::vector<TinyBuffer*> syncBuffers = {}, const VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f }, const VkClearValue depthStencil = { .depthStencil = { 1.0f, 0 } }) {
				VkCommandBufferBeginInfo beginInfo{};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
				beginInfo.pInheritanceInfo = VK_NULL_HANDLE; // Optional

				VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
				if (result != VK_SUCCESS) return result;
                
                renderTarget->TransitionLayoutBarrier(commandBuffer, TinyCmdBufferSubmitStage::STAGE_BEGIN, TinyImageLayout::LAYOUT_COLOR_ATTACHMENT);

				VkRenderingAttachmentInfoKHR colorAttachmentInfo {
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
					.imageView = renderTarget->imageView, .imageLayout = (VkImageLayout) renderTarget->imageLayout,
					.clearValue = clearColor, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				};

				VkRect2D renderAreaKHR {
					.extent = { static_cast<uint32_t>(renderTarget->width), static_cast<uint32_t>(renderTarget->height) }, .offset = { 0,0 }
				};

				VkRenderingInfoKHR dynamicRenderInfo {
					.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
					.renderArea = renderAreaKHR,
					.layerCount = 1,
					.colorAttachmentCount = 1,
					.pColorAttachments = &colorAttachmentInfo
				};

				VkRenderingAttachmentInfoKHR depthStencilAttachmentInfo {};
				if (renderContext.graphicsPipeline.enableDepthTesting) {
                    optionalDepthImage->TransitionLayoutBarrier(commandBuffer, TinyCmdBufferSubmitStage::STAGE_BEGIN, TinyImageLayout::LAYOUT_DEPTHSTENCIL_ATTACHMENT);

                    depthStencilAttachmentInfo = {
						.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
						.imageView = optionalDepthImage->imageView, .imageLayout = (VkImageLayout) optionalDepthImage->imageLayout,
						.clearValue = depthStencil, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					};
					dynamicRenderInfo.pDepthAttachment = &depthStencilAttachmentInfo;
                }

				VkViewport dynamicViewportKHR {
					.x = 0, .y = 0, .minDepth = 0.0f, .maxDepth = 1.0f,
					.width = static_cast<float>(renderTarget->width), .height = static_cast<float>(renderTarget->height),
				};

				vkCmdSetViewport(commandBuffer, 0, 1, &dynamicViewportKHR);
				vkCmdSetScissor(commandBuffer, 0, 1, &renderAreaKHR);
                
                result = vkCmdBeginRenderingEKHR(renderContext.vkdevice.instance, commandBuffer, &dynamicRenderInfo);
				if (result != VK_SUCCESS) return result;
				
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderContext.graphicsPipeline.graphicsPipeline);
                return VK_SUCCESS;
			}

			/// @brief Ends recording render commands to the provided command buffer.
			VkResult EndRecordCmdBuffer(VkCommandBuffer commandBuffer, std::vector<TinyImage*> syncImages = {}, std::vector<TinyBuffer*> syncBuffers = {}, const VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f }, const VkClearValue depthStencil = { .depthStencil = { 1.0f, 0 } }) {
				VkResult result = vkCmdEndRenderingEKHR(renderContext.vkdevice.instance, commandBuffer);
				if (result != VK_SUCCESS) return result;

				renderTarget->TransitionLayoutBarrier(commandBuffer, TinyCmdBufferSubmitStage::STAGE_END, (renderTarget->imageType == TinyImageType::TYPE_SWAPCHAIN)? TinyImageLayout::LAYOUT_PRESENT_SRC : TinyImageLayout::LAYOUT_COLOR_ATTACHMENT);
				if (renderContext.graphicsPipeline.enableDepthTesting)
                    optionalDepthImage->TransitionLayoutBarrier(commandBuffer, TinyCmdBufferSubmitStage::STAGE_END, TinyImageLayout::LAYOUT_DEPTHSTENCIL_ATTACHMENT);

				return vkEndCommandBuffer(commandBuffer);
			}
			
			/// @brief Executes the registered onRenderEvents and renders them to the target image/texture.
			virtual VkResult RenderExecute(bool waitFences = true) {
				if (renderTarget == VK_NULL_HANDLE) return VK_ERROR_INITIALIZATION_FAILED;
				
				if (waitFences) {
					vkWaitForFences(renderContext.vkdevice.logicalDevice, 1, &renderTarget->imageWaitable, VK_TRUE, UINT64_MAX);
					vkResetFences(renderContext.vkdevice.logicalDevice, 1, &renderTarget->imageWaitable);
				}
				
				//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
				if (renderContext.graphicsPipeline.enableDepthTesting)
                    if (optionalDepthImage->width != renderTarget->width || optionalDepthImage->height != renderTarget->height) {
						optionalDepthImage->Disposable(false);
						optionalDepthImage->ReCreateImage(optionalDepthImage->imageType, renderTarget->width, renderTarget->height, renderContext.graphicsPipeline.QueryDepthFormat(), VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
					}
				
				commandPool->ReturnAllBuffers();
                onRenderEvents.invoke(*commandPool);
				//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                
				std::vector<VkCommandBuffer> commandBuffers;
				auto buffers = commandPool->commandBuffers;
				std::for_each(buffers.begin(), buffers.end(),
					[&commandBuffers](std::pair<VkCommandBuffer, VkBool32> cmdBuffer){
						if (cmdBuffer.second) commandBuffers.push_back(cmdBuffer.first);
				});

				VkSubmitInfo submitInfo{};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
				submitInfo.pCommandBuffers = commandBuffers.data();
				
				VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
				VkSemaphore waitSemaphores[] = { renderTarget->imageAvailable };
				VkSemaphore signalSemaphores[] = { renderTarget->imageFinished };
				
				VkResult result;
				if (renderTarget->imageType == TinyImageType::TYPE_SWAPCHAIN) {
					submitInfo.waitSemaphoreCount = 1;
					submitInfo.pWaitDstStageMask = waitStages;
					submitInfo.pWaitSemaphores = waitSemaphores;
					submitInfo.signalSemaphoreCount = 1;
					submitInfo.pSignalSemaphores = signalSemaphores;
					result = vkQueueSubmit(renderContext.graphicsPipeline.presentQueue, 1, &submitInfo, renderTarget->imageWaitable);
				} else {
					result = vkQueueSubmit(renderContext.graphicsPipeline.graphicsQueue, 1, &submitInfo, renderTarget->imageWaitable);
				}

				return result;
			}

			/// @brief Acquires the target's mutex lock and executes the registered onRenderEvents and renders them to the target image/texture.
			VkResult RenderExecuteThreadSafe() {
				TinyTimedGuard imageLock(renderTarget->image_lock);
				if (!imageLock.signaled())
					return VK_ERROR_OUT_OF_DATE_KHR;
				return RenderExecute();
			}

			/// @brief Initialize this Graphics Renderer with its target settings.
			VkResult Initialize() {
				return SetRenderTarget(commandPool, renderTarget, optionalDepthImage, false);
			}

			/// @brief Constructor(...) + Initialize() with error result as combined TinyObject<Object,VkResult>.
			template<typename... A>
			inline static TinyObject<TinyRenderer> Construct(TinyRenderContext& renderContext, TinyCommandPool* cmdPool, TinyImage* renderTarget, TinyImage* optionalDepthImage = VK_NULL_HANDLE) {
				std::unique_ptr<TinyRenderer> object =
					std::make_unique<TinyRenderer>(renderContext, cmdPool, renderTarget, optionalDepthImage);
				return TinyObject<TinyRenderer>(object, object->Initialize());
			}	
        };
    }
#endif

/// 
/// RENDERING PARADIGM:
///		TinyRenderer is for rendering directly to a VkImage render target for offscreen rendering.
///		Call RenderExecute(mutex[optional], preRecordedCommandBuffer[optiona]) to render to the image.
///			You may pass a pre-recorded command buffer ready to go, or retrieve a command buffer from
///			the underlying command pool queue and build your command buffer with a render event (onRenderEvent).
///				onRenderEvent.hook(callback<VkCommandBuffer>(...));
///				
///			If you use a render event, the command buffer will be returned to the command pool queue after execution.
///		
///		TinySwapChainRenderer is for rendering directly to the SwapChain for onscreen rendering.
///		Call RenderExecute(mutex[optional]) to render to the swap chain image.
///			All swap chain rendering is done via render events and does not accept pre-recorded command buffers.
///			The command buffer will be returned to the command pool queue after execution.
///	
///	Why? This rendering paradigm allows the swap chain to effectively manage and synchronize its own resources
///	minimizing screen rendering errors or validation layer errors.
/// 
/// Both the TinyImageRenderer and TinySwapChainRenderer contain their own managed depthImages which will
/// be optionally created and utilized if their underlying graphics pipeline supports depth testing.
/// 
///
/// Vulkan Graphics Pipeline Order:
///		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT = 0x00000001,
///		VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT = 0x00000002,
///		VK_PIPELINE_STAGE_VERTEX_INPUT_BIT = 0x00000004,
///		VK_PIPELINE_STAGE_VERTEX_SHADER_BIT = 0x00000008,
///		VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT = 0x00000010,
///		VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT = 0x00000020,
///		VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT = 0x00000040,
///		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT = 0x00000080,
///		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT = 0x00000100,
///		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT = 0x00000200,
///		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT = 0x00000400,
///		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT = 0x00000800,
///		VK_PIPELINE_STAGE_TRANSFER_BIT = 0x00001000,
///		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT = 0x00002000
///	
///	Top of Pipe
///		Draw Indirect
///
///		Vertex Input
///		Vertex Shader
///
///		Tessellation Shader
///		Geometry Shader
///
///		Fragment Shader
///		Early Fragment Test
///		Late Fragment Test
///		Color Attachement Output
///
///		Transfer
///	Bottom of Pipe
///