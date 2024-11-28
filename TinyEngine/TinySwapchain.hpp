#pragma once
#ifndef TINY_ENGINE_SWAPCHAIN
#define TINY_ENGINE_SWAPCHAIN
	#include "./TinyEngine.hpp"

	namespace TINY_ENGINE_NAMESPACE {
		/// @brief Onscreen Rendering (Render/Present-To-Screen Model): Render to SwapChain.
		class TinySwapchain : public TinyDisposable, public TinyRenderer {
		public:
			std::timed_mutex swapChainMutex;
			TinySurfaceSupporter presentDetails;
			VkSwapchainKHR swapChain = VK_NULL_HANDLE;
			VkFormat imageFormat;
			VkExtent2D imageExtent;
			VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			const TinyBufferingMode bufferingMode;
            size_t cmdPoolBufferCount;
			
			std::vector<TinyImage*> imageSources;
			std::vector<TinyImage*> imageDepthSources;
			std::vector<VkSemaphore> imageAvailable;
			std::vector<VkSemaphore> imageFinished;
			std::vector<VkFence> imageInFlight;
			std::vector<TinyCommandPool*> imageCmdPools;

			uint32_t currentSyncFrame = 0; // Current Synchronized Frame (Ordered).
			uint32_t currentSwapFrame = 0; // Current SwapChain Image Frame (Out of Order).
			std::atomic_bool presentable, refreshable;

			TinyWindow& window;

			inline static TinyInvokable<int, int> onResizeFrameBuffer;

            /// @brief Remove default copy destructor.
			TinySwapchain(const TinySwapchain&) = delete;
            
			/// @brief Remove default copy destructor.
			TinySwapchain operator=(const TinySwapchain&) = delete;
			
			/// @brief Calls the disposable interface dispose event.
			~TinySwapchain() { this->Dispose(); }

			/// @brief Manually calls dispose on resources without deleting the object.
			void Disposable(bool waitIdle) {
				if (waitIdle) renderContext.vkdevice.DeviceWaitIdle();

				if (renderContext.graphicsPipeline.enableDepthTesting) {
					for(TinyImage* depthImage : imageDepthSources) {
						depthImage->Dispose();
						delete depthImage;
					}
				}

				for (TinyCommandPool* cmdPool : imageCmdPools) {
					cmdPool->Dispose();
					delete cmdPool;
				}
				
				for(VkSemaphore available : imageAvailable)
					vkDestroySemaphore(renderContext.vkdevice.logicalDevice, available, VK_NULL_HANDLE);

				for(VkSemaphore finished : imageFinished)
					vkDestroySemaphore(renderContext.vkdevice.logicalDevice, finished, VK_NULL_HANDLE);
				
				for(VkFence inflight : imageInFlight)
					vkDestroyFence(renderContext.vkdevice.logicalDevice, inflight, VK_NULL_HANDLE);

				for(auto swapImage : imageSources) {
					vkDestroyImageView(renderContext.vkdevice.logicalDevice, swapImage->imageView, VK_NULL_HANDLE);
					delete swapImage;
				}

				vkDestroySwapchainKHR(renderContext.vkdevice.logicalDevice, swapChain, VK_NULL_HANDLE);
			}

			/// @brief Creates a renderer specifically for performing render commands on a TinySwapChain (VkSwapChain) to present to the window.
			TinySwapchain(TinyRenderContext& renderContext, TinyWindow& window, const TinyBufferingMode bufferingMode, size_t cmdpoolbuffercount = TinyCommandPool::defaultCommandPoolSize, TinySurfaceSupporter presentDetails = TinySurfaceSupporter(), VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
				: window(window), bufferingMode(bufferingMode), cmdPoolBufferCount(cmdpoolbuffercount), presentDetails(presentDetails), imageUsage(imageUsage), presentable(true), TinyRenderer(renderContext, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE) {
				onDispose.hook(TinyCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));
				onResizeFrameBuffer.hook(TinyCallback<int, int>([this](int, int){ this->RenderSwapChain(); }));
				window.onResizeFrameBuffer.hook(TinyCallback<GLFWwindow*, int, int>([this](GLFWwindow* w, int x, int y) { this->OnFrameBufferResizeCallback(w, x, y); }));
				imageExtent = (VkExtent2D) { static_cast<uint32_t>(window.hwndWidth), static_cast<uint32_t>(window.hwndHeight) };
			}

			/// @brief Create the Vulkan surface swap-chain images and imageviews.
			VkResult CreateSwapChainImages(uint32_t width = 0, uint32_t height = 0) {
				TinySwapChainSupporter swapChainSupport = QuerySwapChainSupport(renderContext.vkdevice.physicalDevice);
				VkSurfaceFormatKHR surfaceFormat = QuerySwapSurfaceFormat(swapChainSupport.formats);
				VkPresentModeKHR presentMode = QuerySwapPresentMode(swapChainSupport.presentModes);
				VkExtent2D extent = QuerySwapExtent(swapChainSupport.capabilities);
				uint32_t imageCount = std::min(swapChainSupport.capabilities.maxImageCount, std::max(swapChainSupport.capabilities.minImageCount, static_cast<uint32_t>(bufferingMode)));

				if (width != 0 && height != 0) {
					extent = {
						std::min(std::max((uint32_t)width, swapChainSupport.capabilities.minImageExtent.width), swapChainSupport.capabilities.maxImageExtent.width),
						std::min(std::max((uint32_t)height, swapChainSupport.capabilities.minImageExtent.height), swapChainSupport.capabilities.maxImageExtent.height)
					};
				} else {
					extent = QuerySwapExtent(swapChainSupport.capabilities);
				}

				if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
					imageCount = swapChainSupport.capabilities.maxImageCount;

				VkSwapchainCreateInfoKHR createInfo{};
				createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
				createInfo.surface = renderContext.vkdevice.presentSurface;
				createInfo.minImageCount = imageCount;
				createInfo.imageFormat = surfaceFormat.format;
				createInfo.imageColorSpace = surfaceFormat.colorSpace;
				createInfo.imageExtent = extent;
				createInfo.imageArrayLayers = 1; // Change when developing VR or other 3D stereoscopic applications
				createInfo.imageUsage = imageUsage;

				TinyQueueFamily indices = renderContext.vkdevice.QueryPhysicalDeviceQueueFamilies();
				if (!indices.hasGraphicsFamily || !indices.hasPresentFamily)
					return VK_ERROR_INITIALIZATION_FAILED;

				uint32_t queueFamilyIndices[] = { indices.graphicsFamily, indices.presentFamily };

				if (indices.graphicsFamily != indices.presentFamily) {
					createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
					createInfo.queueFamilyIndexCount = 2;
					createInfo.pQueueFamilyIndices = queueFamilyIndices;
				} else {
					createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
					createInfo.queueFamilyIndexCount = 0; // Optional
					createInfo.pQueueFamilyIndices = VK_NULL_HANDLE; // Optional
				}

				createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
				createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
				createInfo.presentMode = presentMode;
				createInfo.clipped = VK_TRUE;
				createInfo.oldSwapchain = swapChain;

				if (vkCreateSwapchainKHR(renderContext.vkdevice.logicalDevice, &createInfo, VK_NULL_HANDLE, &swapChain) != VK_SUCCESS)
					return VK_ERROR_INITIALIZATION_FAILED;

				vkGetSwapchainImagesKHR(renderContext.vkdevice.logicalDevice, swapChain, &imageCount, VK_NULL_HANDLE);
				
				std::vector<VkImage> newSwapImages;
				newSwapImages.resize(imageCount);
				vkGetSwapchainImagesKHR(renderContext.vkdevice.logicalDevice, swapChain, &imageCount, newSwapImages.data());

				imageSources.resize(imageCount);
				for(uint32_t i = 0; i < imageCount; i++)
					imageSources[i] = new TinyImage(renderContext, TinyImageType::TYPE_SWAPCHAIN, extent.width, extent.height, newSwapImages[i], VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);

				imageFormat = surfaceFormat.format;
				imageExtent = extent;
				return VK_SUCCESS;
			}

			/// @brief Create the image views for rendering to images (including those in the swap-chain).
			void CreateSwapChainImageViews(VkImageViewCreateInfo* createInfoEx = VK_NULL_HANDLE) {
				for (size_t i = 0; i < imageSources.size(); i++) {
					VkImageViewCreateInfo createInfo{};

					if (createInfoEx == VK_NULL_HANDLE) {
						createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
						createInfo.image = imageSources[i]->image;
						createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
						createInfo.format = imageFormat;

						createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
						createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
						createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
						createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

						createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
						createInfo.subresourceRange.baseMipLevel = 0;
						createInfo.subresourceRange.levelCount = 1;
						createInfo.subresourceRange.baseArrayLayer = 0;
						createInfo.subresourceRange.layerCount = 1;
					} else { createInfo = *createInfoEx; }

					vkCreateImageView(renderContext.vkdevice.logicalDevice, &createInfo, VK_NULL_HANDLE, &imageSources[i]->imageView);
				}
			}

			/// @brief Create the Vulkan surface swapchain.
			VkResult CreateSwapChain(uint32_t width = 0, uint32_t height = 0) {
				VkResult result = CreateSwapChainImages(width, height);
				if (result == VK_SUCCESS) CreateSwapChainImageViews();
				return result;
			}

			/// @brief Creates the synchronization semaphores/fences for swapchain multi-frame buffering.
			VkResult CreateImageSyncObjects() {
				size_t count = static_cast<size_t>(bufferingMode);
				imageAvailable.resize(count);
				imageFinished.resize(count);
				imageInFlight.resize(count);

				VkSemaphoreCreateInfo semaphoreInfo{};
				semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

				VkFenceCreateInfo fenceInfo{};
				fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
				fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

				for (size_t i = 0; i < imageSources.size(); i++) {
					VkResult result = vkCreateSemaphore(renderContext.vkdevice.logicalDevice, &semaphoreInfo, VK_NULL_HANDLE, &imageAvailable[i]);
					if (result != VK_SUCCESS) return result;
					result = vkCreateSemaphore(renderContext.vkdevice.logicalDevice, &semaphoreInfo, VK_NULL_HANDLE, &imageFinished[i]);
					if (result != VK_SUCCESS) return result;
					result = vkCreateFence(renderContext.vkdevice.logicalDevice, &fenceInfo, VK_NULL_HANDLE, &imageInFlight[i]);
					if (result != VK_SUCCESS) return result;
					
					imageSources[i]->imageAvailable = imageAvailable[i];
					imageSources[i]->imageFinished = imageFinished[i];
					imageSources[i]->imageWaitable = imageInFlight[i];
				}

                return VK_SUCCESS;
			}

			/// @brief Checks the VkPhysicalDevice for swap-chain availability.
			TinySwapChainSupporter QuerySwapChainSupport(VkPhysicalDevice device) {
				TinySwapChainSupporter details;

				uint32_t formatCount;
				VkSurfaceKHR windowSurface = renderContext.vkdevice.presentSurface;
				vkGetPhysicalDeviceSurfaceFormatsKHR(device, windowSurface, &formatCount, VK_NULL_HANDLE);
				vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, windowSurface, &details.capabilities);

				if (formatCount > 0) {
					details.formats.resize(formatCount);
					vkGetPhysicalDeviceSurfaceFormatsKHR(device, windowSurface, &formatCount, details.formats.data());
				}

				uint32_t presentModeCount;
				vkGetPhysicalDeviceSurfacePresentModesKHR(device, windowSurface, &presentModeCount, VK_NULL_HANDLE);

				if (presentModeCount != 0) {
					details.presentModes.resize(presentModeCount);
					vkGetPhysicalDeviceSurfacePresentModesKHR(device, windowSurface, &presentModeCount, details.presentModes.data());
				}

				return details;
			}

			/// @brief Gets the swap-chain surface format.
			VkSurfaceFormatKHR QuerySwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
				for (const auto& availableFormat : availableFormats)
					if (availableFormat.format == presentDetails.dataFormat && availableFormat.colorSpace == presentDetails.colorSpace)
						return availableFormat;

				return availableFormats[0];
			}

			/// @brief Select the swap-chains active present mode.
			VkPresentModeKHR QuerySwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
				for (const auto& availablePresentMode : availablePresentModes)
					if (availablePresentMode == presentDetails.idealPresentMode)
						return availablePresentMode;

				return VK_PRESENT_MODE_FIFO_KHR;
			}

			/// @brief Select swap-chain extent (swap-chain surface resolution).
			VkExtent2D QuerySwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
				int width, height;
				window.OnFrameBufferReSizeCallback(width, height);

				VkExtent2D extent = {
					std::min(std::max((uint32_t)width, capabilities.minImageExtent.width), capabilities.maxImageExtent.width),
					std::min(std::max((uint32_t)height, capabilities.minImageExtent.height), capabilities.maxImageExtent.height)
				};

				extent.width = std::max(1u, extent.width);
				extent.height = std::max(1u, extent.height);
				return extent;
			}

			/// @brief Acquires the next image from the swap chain and returns out that image index.
			VkResult QueryNextImage() {
				vkWaitForFences(renderContext.vkdevice.logicalDevice, 1, &imageInFlight[currentSyncFrame], VK_TRUE, UINT64_MAX);
				vkResetFences(renderContext.vkdevice.logicalDevice, 1, &imageInFlight[currentSyncFrame]);
				return vkAcquireNextImageKHR(renderContext.vkdevice.logicalDevice, swapChain, UINT64_MAX, imageAvailable[currentSyncFrame], VK_NULL_HANDLE, &currentSwapFrame);
			}

			/// @brief Present the render results to the screen.
			VkResult RenderPresent() {
				VkSemaphore signalSemaphores[] = { renderTarget->imageFinished };
				
				VkPresentInfoKHR presentInfo{};
				presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
				presentInfo.waitSemaphoreCount = 1;
				presentInfo.pWaitSemaphores = signalSemaphores;

				VkSwapchainKHR swapChainList[]{ swapChain };
				presentInfo.swapchainCount = 1;
				presentInfo.pSwapchains = swapChainList;
				presentInfo.pImageIndices = &currentSwapFrame;

				currentSyncFrame = (currentSyncFrame + 1) % static_cast<size_t>(imageSources.size());
				return vkQueuePresentKHR(renderContext.graphicsPipeline.presentQueue, &presentInfo);
			}
			
			/// @brief Render the swapchain commands.
			VkResult RenderSwapChain() {
				if (refreshable) { OnFrameBufferResizeCallbackNoLock(window.hwndWindow, window.hwndWidth, window.hwndHeight); return VK_SUBOPTIMAL_KHR; }
				if (!presentable) return VK_ERROR_OUT_OF_DATE_KHR;
				
				VkResult result = QueryNextImage();
				TinyImage* swapDepthImage = (renderContext.graphicsPipeline.enableDepthTesting)? imageDepthSources[currentSyncFrame]: VK_NULL_HANDLE;
				
				imageSources[currentSwapFrame]->imageAvailable = imageAvailable[currentSyncFrame];
				imageSources[currentSwapFrame]->imageFinished = imageFinished[currentSyncFrame];
				imageSources[currentSwapFrame]->imageWaitable = imageInFlight[currentSyncFrame];
				
				this->SetRenderTarget(imageCmdPools[currentSyncFrame], imageSources[currentSwapFrame], swapDepthImage, false);

				if (result == VK_SUCCESS) {
					result = this->TinyRenderer::RenderExecute(false);
					if (result == VK_SUCCESS)
						result = this->RenderPresent();
				}
				
				if (result == VK_ERROR_OUT_OF_DATE_KHR) {
					imageCmdPools[currentSyncFrame]->ReturnAllBuffers();
					presentable = false;
					currentSyncFrame = 0;
				} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
					return VK_ERROR_INITIALIZATION_FAILED;
                
				return result;
			}

			/// @brief Notify the render engine that the window's frame buffer needs to be refreshed (without thread locking).
			void OnFrameBufferResizeCallbackNoLock(GLFWwindow* hwndWindow, int width, int height) {
				if (hwndWindow != window.hwndWindow) return;

				if (width > 0 && height > 0) {
					renderContext.vkdevice.DeviceWaitIdle();

					for(auto swapImage : imageSources) {
						vkDestroyImageView(renderContext.vkdevice.logicalDevice, swapImage->imageView, VK_NULL_HANDLE);
						delete swapImage;
					}
					imageSources.resize(0);

					VkSwapchainKHR oldSwapChain = swapChain;
					CreateSwapChain(width, height);
					vkDestroySwapchainKHR(renderContext.vkdevice.logicalDevice, oldSwapChain, VK_NULL_HANDLE);

					presentable = true;
					refreshable = false;
					imageExtent = (VkExtent2D) { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
					onResizeFrameBuffer.invoke(imageExtent.width, imageExtent.height);
				}
			}
			
			/// @brief Notify the render engine that the window's frame buffer needs to be refreshed (with thread locking).
			void OnFrameBufferResizeCallback(GLFWwindow* hwndWindow, int width, int height) {
				TinyTimedGuard swapChainLock(swapChainMutex);
				if (!swapChainLock.signaled()) return;
				OnFrameBufferResizeCallbackNoLock(hwndWindow, width, height);
			}

			/// @brief Returns the current resource synchronized frame index.
			uint32_t GetSyncronizedFrameIndex() { return currentSyncFrame; }
			
			/// @brief Returns reference to presentable atomic_bool (whether swapchain is presentable or not).
			std::atomic_bool& GetPresentableBool() { return presentable; }
			
			/// @brief Returns reference to presentable atomic_bool (whether swapchain is NOT presentable and needs a refresh).
			std::atomic_bool& GetRefreshableBool() { return refreshable; }

			/// @brief Returns the current resource synchronized frame index.
			void PushPresentMode(VkPresentModeKHR presentMode) {
				if (presentMode != presentDetails.idealPresentMode) {
					presentDetails = { presentDetails.dataFormat, presentDetails.colorSpace, presentMode };
					refreshable = true;
				}
			}

			/// @brief Executes the registered onRenderEvents and presents them to the SwapChain(Window).
			VkResult RenderExecute(bool waitFences = true) override {
				TinyTimedGuard swapChainLock(swapChainMutex);
				if (!swapChainLock.signaled())
                    return VK_ERROR_OUT_OF_DATE_KHR;
				return RenderSwapChain();
			}

			/// @brief Initializes this swapchain renderer.
			VkResult Initialize() {
				for(size_t i = 0; i < static_cast<size_t>(bufferingMode); i++) {
					TinyCommandPool* cmdpool = new TinyCommandPool(renderContext.vkdevice, false, cmdPoolBufferCount);
					cmdpool->Initialize();
					imageCmdPools.push_back(cmdpool);
				}
				
				if (renderContext.graphicsPipeline.enableDepthTesting)
					for(size_t i = 0; i < static_cast<size_t>(bufferingMode); i++)
						imageDepthSources.push_back(new TinyImage(renderContext, TinyImageType::TYPE_DEPTHSTENCIL, imageExtent.width, imageExtent.height, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, renderContext.graphicsPipeline.QueryDepthFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT));
				
				VkResult result = CreateSwapChain();
                if (result != VK_SUCCESS)
					return result;
				return CreateImageSyncObjects();
            }
			
			/// @brief Constructor(...) + Initialize() with error result as combined TinyConstruct<Object,VkResult>.
			template<typename... A>
			inline static TinyConstruct<TinySwapchain> Construct(TinyRenderContext& renderContext, TinyWindow& window, const TinyBufferingMode bufferingMode, size_t cmdpoolbuffercount = TinyCommandPool::defaultCommandPoolSize, TinySurfaceSupporter presentDetails = TinySurfaceSupporter(), VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
				std::unique_ptr<TinySwapchain> object =
					std::make_unique<TinySwapchain>(renderContext, window, bufferingMode, cmdpoolbuffercount, presentDetails, imageUsage);
				return TinyConstruct<TinySwapchain>(object, object->Initialize());
			}
		};
	}
#endif

/// 
/// RENDERING PARADIGM:
///		TinyImageRenderer is for rendering directly to a VkImage render target for offscreen rendering.
///		Call RenderExecute(mutex[optional], preRecordedCommandBuffer[optiona]) to render to the image.
///			You may pass a pre-recorded command buffer ready to go, or retrieve a command buffer from
///			the underlying command pool queue and build your command buffer with a render event (onRenderEvent).
///				onRenderEvent += callback<VkCommandBuffer>(...);
///				
///			If you use a render event, the command buffer will be returned to the command pool queue after execution.
///		
///		TinySwapchain is for rendering directly to the SwapChain for onscreen rendering.
///		Call RenderExecute(mutex[optional]) to render to the swap chain image.
///			All swap chain rendering is done via render events and does not accept pre-recorded command buffers.
///			The command buffer will be returned to the command pool queue after execution.
///	
///	Why? This rendering paradigm allows the swap chain to effectively manage and synchronize its own resources
///	minimizing screen rendering errors or validation layer errors.
/// 
/// Both the TinyImageRenderer and TinySwapchain contain their own managed depthImages which will
/// be optionally created and utilized if their underlying graphics pipeline supports depth testing.
/// 