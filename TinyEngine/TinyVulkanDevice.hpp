#pragma once
#ifndef TINY_ENGINE_TinyVkDevice
#define TINY_ENGINE_TinyVkDevice

	#include "./TinyEngine.hpp"
	
	namespace TINY_ENGINE_NAMESPACE {
		/// @brief Vulkan Queue Family flags.
		struct TinyQueueFamily {
			uint32_t graphicsFamily, presentFamily;
			bool hasGraphicsFamily, hasPresentFamily;

			TinyQueueFamily() : graphicsFamily(0), presentFamily(0), hasGraphicsFamily(false), hasPresentFamily(false) {}
			void SetGraphicsFamily(uint32_t queueFamily) { graphicsFamily = queueFamily; hasGraphicsFamily = true; }
			void SetPresentFamily(uint32_t queueFamily) { presentFamily = queueFamily; hasPresentFamily = true; }
		};

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

			const bool useTimestampBit, useGraphicsBit = true, usePresentBit = true;
			VkPhysicalDeviceFeatures deviceFeatures = {};
			VkInstance instance = VK_NULL_HANDLE;
			VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
			VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
			VmaAllocator memoryAllocator = VK_NULL_HANDLE;
			VkDevice logicalDevice = VK_NULL_HANDLE;
			TinyWindow* window = VK_NULL_HANDLE;
			VkSurfaceKHR presentSurface = VK_NULL_HANDLE;
			VkResult initialized = VK_ERROR_INITIALIZATION_FAILED;
			VkPhysicalDeviceProperties2 deviceProperties = {};

			/// @brief Remove default copy destructor.
			TinyVkDevice(const TinyVkDevice&) = delete;
			
			/// @brief Remove default copy destructor.
			TinyVkDevice operator=(const TinyVkDevice&) = delete;

			/// @brief Calls the disposable interface dispose event.
			~TinyVkDevice() { this->Dispose(); }

			/// @brief Manually calls dispose on resources without deleting the object.
			void Disposable(bool waitIdle) {
				if (waitIdle) DeviceWaitIdle();
				if (memoryAllocator != VK_NULL_HANDLE) vmaDestroyAllocator(memoryAllocator);
				if (logicalDevice != VK_NULL_HANDLE) vkDestroyDevice(logicalDevice, VK_NULL_HANDLE);
				if (presentSurface != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance, presentSurface, VK_NULL_HANDLE);
				if (debugMessenger != VK_NULL_HANDLE) DestroyDebugUtilsMessengerEXT(instance, debugMessenger, VK_NULL_HANDLE);
				if (instance != VK_NULL_HANDLE) vkDestroyInstance(instance, VK_NULL_HANDLE);
			}

			/// @brief Create managed VkDevice via Vulkan API. Initializes Vulkan: Must call Initialize() manually.
			TinyVkDevice(bool useTimestampBit = false, TinyWindow* window = VK_NULL_HANDLE, VkPhysicalDeviceFeatures deviceFeatures = { .multiDrawIndirect = VK_TRUE })
			: window(window), useTimestampBit(useTimestampBit), deviceFeatures(deviceFeatures) {
				onDispose.hook(TinyCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));
				useTimestampBit = TINY_ENGINE_VALIDATION;
				initialized = Initialize();
			}

			/// @brief Wait for GPU device to finish transfer/render commands.
			VkResult DeviceWaitIdle() { return vkDeviceWaitIdle(logicalDevice); }

			/// @brief Returns info about the VkPhysicalDevice graphics/present queue families. If no surface provided, auto checks for Win32 surface support.
			TinyQueueFamily QueryPhysicalDeviceQueueFamilies(VkPhysicalDevice device = VK_NULL_HANDLE) {
				device = (device == VK_NULL_HANDLE)? physicalDevice : device;
				if (device == VK_NULL_HANDLE) return {};

				std::vector<VkQueueFamilyProperties> queueFamilies;
				QueryQueueFamilyProperties(physicalDevice, queueFamilies);
				
				int32_t useOutputBits = static_cast<int32_t>(useGraphicsBit) + static_cast<int32_t>(usePresentBit) + static_cast<int32_t>(useTimestampBit);
				TinyQueueFamily indices = {};
				for (int i = 0; i < queueFamilies.size(); i++) {
					VkBool32 presentSupport = false;
					vkGetPhysicalDeviceSurfaceSupportKHR(device, i, presentSurface, &presentSupport);
					VkBool32 timestampSupport = (useTimestampBit)? queueFamilies[i].timestampValidBits : 1;
					if (useGraphicsBit && !indices.hasGraphicsFamily && queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && timestampSupport > 0) { indices.SetGraphicsFamily(i); useOutputBits --; }
					if (usePresentBit && !indices.hasPresentFamily && presentSupport && timestampSupport > 0) { indices.SetPresentFamily(i); useOutputBits --; }
				}
				return indices;
			}

			/// @brief Returns an VkDeviceSize ranking of VK_PHYSICAL_DEVICE_TYPE for a VkPhysicalDevice ranked by memory heap size.
			VkDeviceSize QueryPhysicalDeviceRankByHeapSize(VkPhysicalDevice device) {
				VkPhysicalDeviceMemoryProperties2 memoryProperties { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };
				vkGetPhysicalDeviceMemoryProperties2(device, &memoryProperties);
				for(VkMemoryHeap heap : memoryProperties.memoryProperties.memoryHeaps)
					if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) return heap.size;
				return static_cast<VkDeviceSize>(0);
			}

			/// @brief Returns the highest ranked GPU by device memory heap size (regardless of compatibility of settings) and creates its VMA allocator.
			VkResult CreatePhysicalDevice() {
				std::vector<VkPhysicalDevice> suitableDevices;
				QueryPhysicalDevices(instance, suitableDevices);
				std::sort(suitableDevices.begin(), suitableDevices.end(), [this](VkPhysicalDevice A, VkPhysicalDevice B) { return QueryPhysicalDeviceRankByHeapSize(A) >= QueryPhysicalDeviceRankByHeapSize(B); });
				physicalDevice = (suitableDevices.size() > 0)? suitableDevices.front() : VK_NULL_HANDLE;
				return (physicalDevice == VK_NULL_HANDLE)? VK_ERROR_DEVICE_LOST : VK_SUCCESS;
			}

			/// @brief Creates the underlying Vulkan Instance w/ Required Extensions.
			VkResult CreateVkInstance() {
				if (window != VK_NULL_HANDLE)
					for (const auto& extension : window->QueryRequiredExtensions()) instanceExtensions.push_back(extension);
				
				#if TINY_ENGINE_VALIDATION
					instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
					validationLayers.push_back(VK_VALIDATION_LAYER_KHRONOS_EXTENSION_NAME);
					std::cout << "TinyEngine: Enabled Validation Layers & Instance Extensions" << std::endl;
					for(const char* layer : validationLayers)       std::cout << "\tValdLyr:                 " << layer << std::endl;
					for(const auto& extension : instanceExtensions) std::cout << "\tInstExt:                 " << extension << std::endl;
				#endif
				
				VkInstanceCreateInfo createInfo {
					.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
					.pApplicationInfo = &defaultAppInfo,
					.enabledLayerCount = static_cast<uint32_t>(validationLayers.size()), .ppEnabledLayerNames = validationLayers.data(),
					.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size()), .ppEnabledExtensionNames = instanceExtensions.data(),
					.pNext = &defaultDebugCreateInfo
				};

				VkResult result = vkCreateInstance(&createInfo, VK_NULL_HANDLE, &instance);
				if (result != VK_SUCCESS) return result;

				if (window != VK_NULL_HANDLE) presentSurface = window->CreateWindowSurface(instance);	
				
				#if TINY_ENGINE_VALIDATION
					result = CreateDebugUtilsMessengerEXT(instance, &defaultDebugCreateInfo, VK_NULL_HANDLE, &debugMessenger);
				#endif
				return result;
			}

			/// @brief Creates the logical devices for the graphics/present queue families.
			VkResult CreateLogicalDevice() {
				if (physicalDevice == VK_NULL_HANDLE)
					return VK_ERROR_DEVICE_LOST;

				std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
				TinyQueueFamily indices = QueryPhysicalDeviceQueueFamilies(physicalDevice);
				std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily, indices.presentFamily };

				float queuePriority = 1.0f;
				for (uint32_t queueFamily : uniqueQueueFamilies)
					queueCreateInfos.push_back({ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = queueFamily, .queueCount = 1, .pQueuePriorities = &queuePriority });
				
				VkDeviceCreateInfo createInfo {
					.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
					.pQueueCreateInfos = queueCreateInfos.data(), .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
					.pEnabledFeatures = &deviceFeatures, .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()), .ppEnabledExtensionNames = deviceExtensions.data(),
					.pNext = &defaultDynamicRenderingCreateInfo
				};

				VkResult result = vkCreateDevice(physicalDevice, &createInfo, VK_NULL_HANDLE, &logicalDevice);
				if (result != VK_SUCCESS) return result;

				#if TINY_ENGINE_VALIDATION
					std::cout << "TinyEngine: Enabled Device Extensions" << std::endl;
					for (const auto& extension : deviceExtensions) std::cout << "\tDevcExt:                 " << extension << std::endl;
				#endif
				
				VmaAllocatorCreateInfo allocatorCreateInfo { .vulkanApiVersion = TINY_ENGINE_VERSION, .physicalDevice = physicalDevice, .device = logicalDevice, .instance = instance };
				return vmaCreateAllocator(&allocatorCreateInfo, &memoryAllocator);
			}
			
			/// @brief Initializes the Vulkan Instance, Creates VMAllocator and Queries required Logical/Physical Device(s).
			VkResult Initialize() {
				VkResult result = VK_SUCCESS;
				if ((result = CreateVkInstance()) != VK_SUCCESS) return result;
				if ((result = vkCmdRenderingGetCallbacks(instance)) != VK_SUCCESS) return result;
				if ((result = CreatePhysicalDevice()) != VK_SUCCESS) return result;
				result = CreateLogicalDevice();

				#if TINY_ENGINE_VALIDATION
					VkPhysicalDevicePushDescriptorPropertiesKHR pushDescriptorProperties = defaultPushDescriptorProperties;
					deviceProperties = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &pushDescriptorProperties };
					vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties);
					TinyQueueFamily indices = QueryPhysicalDeviceQueueFamilies(physicalDevice);
					std::cout << "TinyEngine: GPU Device Info" << std::endl;
					std::cout << "\tValid Logical Device:    " << (result == VK_SUCCESS?"True":"False") << std::endl;
					std::cout << "\tPhysical Device Name:    " << deviceProperties.properties.deviceName << std::endl;
					std::cout << "\tDevice Rank / Heap Size: " << (QueryPhysicalDeviceRankByHeapSize(physicalDevice) / 1000000000) << " GB" << std::endl;
					std::cout << "\tPush Constant Memory:    " << deviceProperties.properties.limits.maxPushConstantsSize << " Bytes" << std::endl;
					std::cout << "\tPush Descriptor Memory:  " << pushDescriptorProperties.maxPushDescriptors << " Count" << std::endl;
					std::cout << "\tGraphics/Present Pipes:  " << (indices.hasGraphicsFamily?"True":"False") << " / " << (indices.hasPresentFamily?"True":"False") << std::endl;
				#endif
				return result;
			}
		};
	}

#endif