#pragma once
#ifndef TINY_ENGINE_LIBRARY
#define TINY_ENGINE_LIBRARY

    ///   TINYVULKAN LIBRARY DEPENDENCIES: VULKAN, GLFW and VMA compiled library binaries.
    /// 
    ///    C/C++ | Code Configuration | Runtime Libraries:
    ///        RELEASE: /MD
    ///        DEBUG  : /mtD
    ///    C/C++ | General | Additional Include Directories: (DEBUG & RELEASE)
    ///        ..\VulkanSDK\1.3.239.0\Include;...\glfw-3.3.7.bin.WIN64\glfw-3.3.7.bin.WIN64\include;
    ///    Linker | General | Additional Library Directories: (DEBUG & RELEASE)
    ///        ...\glfw-3.3.7.bin.WIN64\lib-vc2022;$(VULKAN_SDK)\Lib; // C:\VulkanSDK\1.3.239.0\Lib
    ///    Linker | Input | Additional Dependencies:
    ///        RELEASE: vulkan-1.lib;glfw3.lib;
    ///        DEBUG  : vulkan-1.lib;glfw3_mt.lib;
    ///    Linker | System | SubSystem
    ///        RELEASE: Windows (/SUBSYSTEM:WINDOWS)
    ///        RELEASE: Console (/SUBSYSTEM:CONSOLE)
    ///        DEBUG  : Console (/SUBSYSTEM:CONSOLE)
    /// 
    ///    * If Debug mode is enabled the console window will be visible, but not on Release.
    ///    * If compiled as RELEASE with console enabled compile with /subsystem:console.
    ///    * Make sure you've installed the SDK bionaries for Vulkan, GLFW and VMA.
    ///    
    ///    VULKAN DEVICE EXTENSIONS:
    ///         Dynamic Rendering Dependency.
    ///              VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME
    ///         Depth fragment testing (discard fragments if fail depth test, if enabled in the graphics pipeline).
    ///              VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME
    ///     	   Allows for rendering without framebuffers and render passes.
    ///              VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
    ///         Allows for writing descriptors directly into a command buffer rather than allocating from sets / pools.
    ///              VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME
    ///         Swapchain support for buffering frame images with the device driver to reduce tearing.
    ///         Only gets added if a window is added to the VkInstance on create via call to glfwGetRequiredInstanceExtensions.
    ///            * VK_KHR_SWAPCHAIN_EXTENSION_NAME

    #define GLFW_INCLUDE_VULKAN
    #if defined (_WIN32)
        #define GLFW_EXPOSE_NATIVE_WIN32
        #define VK_USE_PLATFORM_WIN32_KHR
    #endif
    #include <GLFW/glfw3.h>
    #include <GLFW/glfw3native.h>
    #include <vulkan/vulkan.h>
    
    #define VK_VALIDATION_LAYER_KHRONOS_EXTENSION_NAME "VK_LAYER_KHRONOS_validation"
    #ifdef _DEBUG
        #define TINY_ENGINE_WINDOWMAIN main(int argc, char* argv[])
        #define TINY_ENGINE_VALIDATION VK_TRUE
    #else
        #define TINY_ENGINE_VALIDATION VK_FALSE
        #ifdef _RELEASE
			/// COMPILING WITH CLANG-CL / LLVM
            #define TINY_ENGINE_WINDOWMAIN __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
        #else
            /// For debugging w/ Console on Release change from /WINDOW to /CONSOLE: Linker -> System -> Subsystem.
            #define TINY_ENGINE_WINDOWMAIN main(int argc, char* argv[])
        #endif
    #endif
    
    #ifndef TINY_ENGINE_NAMESPACE
        #define TINY_ENGINE_NAMESPACE tny
        namespace TINY_ENGINE_NAMESPACE {}
    #endif
    #define TINY_ENGINE_MAKE_VERSION(major, minor, patch) VK_MAKE_API_VERSION(0, major, minor, patch)
    #define TINY_ENGINE_VERSION TINY_ENGINE_MAKE_VERSION(1, 0, 0)
    #define TINY_ENGINE_NAME "TINY_ENGINE_LIBRARY"
    
    ///
    /// Enables VMA automated GPU memory management.
    ///
    #define VMA_IMPLEMENTATION
    #define VMA_DEBUG_GLOBAL_MUTEX VK_TRUE
    #define VMA_USE_STL_CONTAINERS VK_TRUE
    #define VMA_RECORDING_ENABLED TINY_ENGINE_VALIDATION
    #include <vma/vk_mem_alloc.h>

    ///
    /// Enables GLM (Standardized Math via OpenGL Mathematics, allows for byte aligned memory).
    ///
    #define GLM_FORCE_RADIANS
    #define GLM_FORCE_LEFT_HANDED
    #define GLM_FORCE_DEPTH_ZERO_TO_ONE
    #define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
    #include <glm/glm.hpp>
    #include <glm/ext.hpp>
    
    ///
    /// General include libraries (data-structs, for-each search, etc.).
    ///
    #include <memory>
    #include <mutex>
    #include <fstream>
    #include <iostream>
    #include <vector>
    #include <array>
    #include <set>
    #include <string>
    #include <algorithm>
    #include <functional>
    #include <utility>
    #include <type_traits>

    #pragma region BACKEND_SYSTEMS
        #include "./Utilities/TinyEnums.hpp"
        #include "./Utilities/TinyTimedGuard.hpp"
        #include "./Utilities/TinyInvokableCallback.hpp"
        #include "./Utilities/TinyDisposable.hpp"
        #include "./Utilities/TinyUtilities.hpp"
    #pragma endregion
    #pragma region WINDOW_INPUT_HANDLING
        #include "./TinyWindow.hpp"
    #pragma endregion
    #pragma region VULKAN_INITIALIZATION
        #include "./TinyVulkanDevice.hpp"
        #include "./TinyCommandPool.hpp"
        #include "./TinyPipeline.hpp"
    #pragma endregion
    #pragma region TINY_RENDERING
        #include "./TinyBuffer.hpp"
        #include "./TinyImage.hpp"
        #include "./TinySwapchain.hpp"
        #include "./TinyRenderCmd.hpp"
        #include "./TinyRenderGraph.hpp"
        #include "./TinyMath.hpp"
    #pragma endregion
#endif

/// 
/// API OUTLINE:
///     Objects which dynamically allocate memory (such as VMA, pipelines, logical devices, etc.)
///     all extent TinyDisposable and call their own dispose/destructor functions.
/// 
///     All objects which extend TinyDisposable expose a Constructor(...) and Initilize() function
///     which can be called as a static constructor:
///     
///         TinyObject<TinyObject> unique_object = TinyObject::Construct(args...);
///             std::unique_ptr<TinyObject>& object = unique_object.source;
///             VkResult = object.result;
///     
///     This static constructor is for creating unique objects which also return ERROR codes
///     rather than relying on exception handling.
/// 
///     Objects can also have their underlying dynamic memory manually disposed by calling:
///         
///         object.Disposable(waitIdle = true/false);
///     
///     Manually disposing dynamic memory keeps the object allive in the event that you need
///     to re-create its resources with different settings / input arguments.
///
///     Manually instantiating objects via their constructors rather than using TinyObject<T>
///     means you'll manually need to call .Initialize() to actually initialize the object and get
///     it's VkResult for error handling.
///
///     Calling the TinyObject::Construction() will call the class' .Initialize() function as well
///     The .Initialize() function calls ALL .Create*() functions of each class.
/// 