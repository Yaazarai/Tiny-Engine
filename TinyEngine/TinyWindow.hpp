#pragma once
#ifndef TINY_ENGINE_TINYVKWINDOW
#define TINY_ENGINE_TINYVKWINDOW
    #include "./TinyEngine.hpp"

    namespace TINY_ENGINE_NAMESPACE {
		class TinyWindow : public TinyDisposable {
		public:
            bool hwndResizable, hwndMinSize, hwndTransparent;
			int hwndWidth, hwndHeight, hwndXpos, hwndYpos;
            int minWidth, minHeight;
            std::string hwndTitle;
			
			/// @brief GLFW window identifier handle (pointer).
            GLFWwindow* hwndWindow;
			
			/// @brief Executes functions in the main window loop (w/ ref to bool to exit loop as needed).
			TinyInvokable<std::atomic_bool&> onWhileMain;
			
			/// @brief Invokable callback to respond to GLFW window API when the window resizes.</suimmary>
			inline static TinyInvokable<GLFWwindow*, int, int> onWindowResized;
			
			/// @brief Invokable callback to respond to GLFW window API when the window moves.</suimmary>
			inline static TinyInvokable<GLFWwindow*, int, int> onWindowPositionMoved;
			
			/// @brief Invokable callback to respond to Vulkan API when the active frame buffer is resized.</suimmary>
			inline static TinyInvokable<GLFWwindow*, int, int> onResizeFrameBuffer;
			
			/// @brief Remove default copy destructor.
			TinyWindow(const TinyWindow&) = delete;
			
			/// @brief Remove default copy destructor.
			TinyWindow& operator=(const TinyWindow&) = delete;
			
			/// @brief Calls the disposable interface dispose event.
			~TinyWindow() { this->Dispose(); }

			/// @brief Disposable function for disposable class interface and window resource cleanup.
			void Disposable(bool waitIdle) {
				glfwDestroyWindow(hwndWindow);
				glfwTerminate();
			}

			/// @brief Create managed GLFW Window and Vulkan API. Initializes GLFW: Must call Initialize() manually.
			TinyWindow(std::string title, int width, int height, bool resizable, bool transparent = false, bool hasMinSize = false, int minWidth = 200, int minHeight = 200)
			: hwndResizable(resizable), hwndTransparent(transparent), hwndMinSize(hasMinSize), hwndWidth(width), hwndHeight(height), minWidth(minWidth), minHeight(minHeight), hwndTitle(title), hwndWindow(VK_NULL_HANDLE) {
				onDispose.hook(TinyCallback<bool>([this](bool forceDispose){this->Disposable(forceDispose); }));
				onWindowResized.hook(TinyCallback<GLFWwindow*, int, int>([this](GLFWwindow* hwnd, int width, int height) { if (hwnd != hwndWindow) return; hwndWidth = width; hwndHeight = height; }));
				onWindowPositionMoved.hook(TinyCallback<GLFWwindow*, int, int>([this](GLFWwindow* hwnd, int xpos, int ypos) { if (hwnd != hwndWindow) return; hwndXpos = xpos; hwndYpos = ypos; }));
				glfwInit();
			}

			/// @brief Generates an event for window framebuffer resizing.
			inline static void OnFrameBufferNotifyReSizeCallback(GLFWwindow* hwnd, int width, int height) {
				onResizeFrameBuffer.invoke(hwnd, width, height);
				onWindowResized.invoke(hwnd, width, height);
			}

			/// @brief Generates an event for window position moved.
			inline static void OnWindowPositionCallback(GLFWwindow* hwnd, int xpos, int ypos) {
				onWindowPositionMoved.invoke(hwnd, xpos, ypos);
			}

			/// @brief Pass to render engine for swapchain resizing.
			void OnFrameBufferReSizeCallback(int& width, int& height) {
				width = 0;
				height = 0;

				while (width <= 0 || height <= 0)
					glfwGetFramebufferSize(hwndWindow, &width, &height);

				hwndWidth = width;
				hwndHeight = height;
			}

			/// @brief Checks if the GLFW window should continue executing (true) or close (false).
			bool ShouldExecute() { return glfwWindowShouldClose(hwndWindow) != GLFW_TRUE; }

			/// @brief Creates a Vulkan surface for this GLFW window.
			VkSurfaceKHR CreateWindowSurface(VkInstance instance) {
				VkSurfaceKHR wndSurface;
                VkResult result = glfwCreateWindowSurface(instance, hwndWindow, VK_NULL_HANDLE, &wndSurface);
				return (result == VK_SUCCESS)? wndSurface : VK_NULL_HANDLE;
			}

			/// @brief Gets the required GLFW extensions.
			inline static std::vector<const char*> QueryRequiredExtensions() {
				glfwInit();
				uint32_t glfwExtensionCount = 0;
				const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
				return std::vector<const char*>(glfwExtensions, glfwExtensions + glfwExtensionCount);
			}

			/// @brief Executes the main window loop.
			void WhileMain(TinyWindowEvents eventType) {
				std::atomic_bool close = true;
				while (close = ShouldExecute()) {
					onWhileMain.invoke(close);
					if (eventType == TinyWindowEvents::POLL_EVENTS)
					{ glfwPollEvents(); } else { glfwWaitEvents(); }
				}
			}

            /// @brief Initializes and Displays the GLFW window and sets its properties.
			VkResult Initialize() {
				glfwInit();
                if (glfwVulkanSupported()) {
                    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
                    glfwWindowHint(GLFW_RESIZABLE, (hwndResizable) ? GLFW_TRUE : GLFW_FALSE);
                    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, (hwndTransparent) ? GLFW_TRUE : GLFW_FALSE);

                    hwndWindow = glfwCreateWindow(hwndWidth, hwndHeight, hwndTitle.c_str(), VK_NULL_HANDLE, VK_NULL_HANDLE);
                    glfwSetWindowUserPointer(hwndWindow, this);
                    glfwSetFramebufferSizeCallback(hwndWindow, TinyWindow::OnFrameBufferNotifyReSizeCallback);
                    glfwSetWindowPosCallback(hwndWindow, TinyWindow::OnWindowPositionCallback);

                    if (hwndMinSize) glfwSetWindowSizeLimits(hwndWindow, minWidth, minHeight, GLFW_DONT_CARE, GLFW_DONT_CARE);
                    return VK_SUCCESS;
                }

                return VK_ERROR_INITIALIZATION_FAILED;
            }
			
			/// @brief Constructor(...) + Initialize() with error result as combined TinyObject<Object,VkResult>.
			template<typename... A>
			inline static TinyObject<TinyWindow> Construct(std::string title, int width, int height, bool resizable, bool transparent  = false, bool hasMinSize = false, int minWidth = 200, int minHeight = 200) {
				std::unique_ptr<TinyWindow> object =
					std::make_unique<TinyWindow>(title, width, height, resizable, transparent, hasMinSize, minWidth, minHeight);
				return TinyObject<TinyWindow>(object, object->Initialize());
			}
        };
    }
#endif