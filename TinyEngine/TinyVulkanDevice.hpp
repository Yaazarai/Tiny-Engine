#pragma once
#ifndef TINY_ENGINE_TinyVkDevice
#define TINY_ENGINE_TinyVkDevice

	#include "./TinyEngine.hpp"
	
	namespace TINY_ENGINE_NAMESPACE {
		/// @brief Vulkan Instance & Render(Physical/Logical) Device & VMAllocator Loader.
		class TinyVkDevice : public TinyDisposable {
		public:
			std::vector<const char*> deviceExtensions = {
				VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
				VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
				VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
				VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
				VK_KHR_SWAPCHAIN_EXTENSION_NAME,
				VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME
			}, validationLayers = {}, instanceExtensions = {};
			VkPhysicalDeviceFeatures deviceFeatures = {};
            VkPhysicalDeviceProperties2 deviceProperties = {};

            TinyWindow* window = VK_NULL_HANDLE;
			VkInstance instance = VK_NULL_HANDLE;
			VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
			VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
			VkDevice logicalDevice = VK_NULL_HANDLE;
            VmaAllocator memoryAllocator = VK_NULL_HANDLE;
			VkSurfaceKHR presentSurface = VK_NULL_HANDLE;
			TinyQueueFamily queueFamilyIndices = {};
            VkResult initialized = VK_ERROR_INITIALIZATION_FAILED;

			TinyVkDevice(const TinyVkDevice&) = delete;
			TinyVkDevice operator=(const TinyVkDevice&) = delete;
			~TinyVkDevice() { this->Dispose(); }
			
			/// @brief Manually calls dispose on resources without deleting the object.
			void Disposable(bool waitIdle) {
				if (waitIdle) vkDeviceWaitIdle(logicalDevice);
				if (memoryAllocator != VK_NULL_HANDLE) vmaDestroyAllocator(memoryAllocator);
				if (logicalDevice != VK_NULL_HANDLE) vkDestroyDevice(logicalDevice, VK_NULL_HANDLE);
				if (presentSurface != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance, presentSurface, VK_NULL_HANDLE);
				if (debugMessenger != VK_NULL_HANDLE) DestroyDebugUtilsMessengerEXT(instance, debugMessenger, VK_NULL_HANDLE);
				if (instance != VK_NULL_HANDLE) vkDestroyInstance(instance, VK_NULL_HANDLE);
			}

			/// @brief Create managed VkDevice via Vulkan API. Automatically calls Initialize().
			TinyVkDevice(TinyWindow* window = VK_NULL_HANDLE, VkPhysicalDeviceFeatures deviceFeatures = { .multiDrawIndirect = VK_TRUE })
			: window(window), deviceFeatures(deviceFeatures) {
				onDispose.hook(TinyCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));
				initialized = Initialize();
			}

			/// @brief Creates the underlying Vulkan Instance w/ Required Extensions.
			VkResult CreateVkInstance() {
				if (window != VK_NULL_HANDLE)
					for (const auto& extension : window->QueryRequiredExtensions()) instanceExtensions.push_back(extension);
				
				#if TINY_ENGINE_VALIDATION
					instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
					validationLayers.push_back(VK_VALIDATION_LAYER_KHRONOS_EXTENSION_NAME);
				#endif
				
				VkInstanceCreateInfo createInfo {
					.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
					.enabledLayerCount = static_cast<uint32_t>(validationLayers.size()), .ppEnabledLayerNames = validationLayers.data(),
					.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size()), .ppEnabledExtensionNames = instanceExtensions.data(),
					.pApplicationInfo = &defaultAppInfo, .pNext = &defaultDebugCreateInfo
				};

				VkResult result = vkCreateInstance(&createInfo, VK_NULL_HANDLE, &instance);
				if (result != VK_SUCCESS) return result;

				presentSurface = window->CreateWindowSurface(instance);
				return CreateDebugUtilsMessengerEXT(instance, &defaultDebugCreateInfo, VK_NULL_HANDLE, &debugMessenger);
			}

			/// @brief Returns the highest ranked GPU by device memory heap size (regardless of compatibility of settings) and creates its VMA allocator.
			VkResult CreatePhysicalDevice() {
				std::vector<VkPhysicalDevice> suitableDevices;
				QueryPhysicalDevices(instance, suitableDevices);
				std::sort(suitableDevices.begin(), suitableDevices.end(), [this](VkPhysicalDevice A, VkPhysicalDevice B) { return QueryPhysicalDeviceRankByHeapSize(A) >= QueryPhysicalDeviceRankByHeapSize(B); });
				physicalDevice = (suitableDevices.size() > 0)? suitableDevices.front() : VK_NULL_HANDLE;
				return (physicalDevice == VK_NULL_HANDLE)? VK_ERROR_DEVICE_LOST : VK_SUCCESS;
			}

			/// @brief Creates the logical devices for the graphics/present queue families.
			VkResult CreateLogicalDevice() {
				if (physicalDevice == VK_NULL_HANDLE) return VK_ERROR_DEVICE_LOST;

				std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
				queueFamilyIndices = QueryPhysicalDeviceQueueFamilies(physicalDevice, presentSurface);
				std::set<uint32_t> uniqueQueueFamilies = { queueFamilyIndices.graphicsFamily, queueFamilyIndices.presentFamily };
                if (!queueFamilyIndices.hasGraphicsFamily || !queueFamilyIndices.hasPresentFamily) return VK_ERROR_INITIALIZATION_FAILED;

				float queuePriority = 1.0f;
				for (uint32_t queueFamily : uniqueQueueFamilies)
					queueCreateInfos.push_back({ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1, .queueFamilyIndex = queueFamily, .pQueuePriorities = &queuePriority });
				
				VkDeviceCreateInfo createInfo {
					.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
					.pQueueCreateInfos = queueCreateInfos.data(), .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
					.ppEnabledExtensionNames = deviceExtensions.data(), .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
					.pEnabledFeatures = &deviceFeatures, .pNext = &defaultDynamicRenderingCreateInfo
				};

				return vkCreateDevice(physicalDevice, &createInfo, VK_NULL_HANDLE, &logicalDevice);
			}

			/// @brief Creates the VMA memory allocator for handling GPU allocated memory.
			VkResult CreateMemoryAllocator() {
				VmaAllocatorCreateInfo allocatorCreateInfo { .vulkanApiVersion = TINY_ENGINE_VERSION, .physicalDevice = physicalDevice, .device = logicalDevice, .instance = instance };
				return vmaCreateAllocator(&allocatorCreateInfo, &memoryAllocator);
			}

			/// @brief Initializes the Vulkan Instance, Creates VMAllocator and Queries required Logical/Physical Device(s).
			VkResult Initialize() {
				VkResult result = VK_SUCCESS;
				if ((result = CreateVkInstance()) != VK_SUCCESS) return result;
				if ((result = vkCmdRenderingGetCallbacks(instance)) != VK_SUCCESS) return result;
				if ((result = CreatePhysicalDevice()) != VK_SUCCESS) return result;
				if ((result = CreateLogicalDevice()) != VK_SUCCESS) return result;
				result = CreateMemoryAllocator();

				#if TINY_ENGINE_VALIDATION
					VkPhysicalDevicePushDescriptorPropertiesKHR pushDescriptorProperties = defaultPushDescriptorProperties;
					deviceProperties = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &pushDescriptorProperties };
					vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties);
					std::cout << "TinyEngine: GPU Device Info" << std::endl;
					std::cout << "\tValid Logical Device:    " << (result == VK_SUCCESS?"True":"False") << std::endl;
					std::cout << "\tPhysical Device Name:    " << deviceProperties.properties.deviceName << std::endl;
					std::cout << "\tDevice Rank / Heap Size: " << (QueryPhysicalDeviceRankByHeapSize(physicalDevice) / 1000000000) << " GB" << std::endl;
					std::cout << "\tPush Constant Memory:    " << deviceProperties.properties.limits.maxPushConstantsSize << " Bytes" << std::endl;
					std::cout << "\tPush Descriptor Memory:  " << pushDescriptorProperties.maxPushDescriptors << " Count" << std::endl;
				#endif
				return result;
			}
        };
    }

#endif