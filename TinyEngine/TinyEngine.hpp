#pragma once
#ifndef TNY_LIBRARY
#define TNY_LIBRARY

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
    
    #ifdef _DEBUG
        #define TNY_WINDOWMAIN main(int argc, char* argv[])
        #define TNY_VALIDATION VK_TRUE
    #else
        #define TNY_VALIDATION VK_FALSE
        #ifdef _WIN32
			#ifndef __clang__
			/// COMPILING WITH VISUAL STUDIO 2022
            #define TNY_WINDOWMAIN __stdcall wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
			#else
			/// COMPILING WITH CLANG-CL / LLVM
            #define TNY_WINDOWMAIN __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
            #endif
        #else
            /// For debugging w/ Console on Release change from /WINDOW to /CONSOLE: Linker -> System -> Subsystem.
            #define TNY_WINDOWMAIN main(int argc, char* argv[])
        #endif
    #endif
    
    #ifndef TNY_NAMESPACE
        #define TNY_NAMESPACE tny
        namespace TNY_NAMESPACE {}
    #endif
    #define TNY_MAKE_VERSION(major, minor, patch) ((((uint32_t)major)<<16)|(((uint32_t)minor)<<8)|((uint32_t)patch))
    #define TNY_VERSION VK_MAKE_API_VERSION(1, 0, 0)
    #define TNY_NAME "TINY_ENGINE_LIBRARY"
    
    ///
    /// Enables VMA automated GPU memory management.
    ///
    #define VMA_IMPLEMENTATION
    #define VMA_DEBUG_GLOBAL_MUTEX VK_TRUE
    #define VMA_USE_STL_CONTAINERS VK_TRUE
    #define VMA_RECORDING_ENABLED TNY_VALIDATION
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
    #include <fstream>
    #include <iostream>
    #include <array>
    #include <set>
    #include <string>
    #include <vector>
    #include <algorithm>

    #pragma region BACKEND_SYSTEMS
    #pragma endregion
    #pragma region WINDOW_INPUT_HANDLING
    #pragma endregion
    #pragma region VULKAN_INITIALIZATION
    #pragma endregion
    #pragma region TNY_RENDERING
    #pragma endregion
#endif