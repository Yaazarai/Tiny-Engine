#pragma once
#ifndef TINY_ENGINE_TinyVkDevice
#define TINY_ENGINE_TinyVkDevice

	#include "./TinyEngine.hpp"
	
	namespace TINY_ENGINE_NAMESPACE {
		#define VK_VALIDATION_LAYER_KHRONOS_EXTENSION_NAME "VK_LAYER_KHRONOS_validation"

		struct TinyQueueFamily {
			uint32_t graphicsFamily, presentFamily, computeFamily;
			bool hasGraphicsFamily, hasPresentFamily, hasComputeFamily;

			TinyQueueFamily() : graphicsFamily(0), presentFamily(0), computeFamily(0), hasGraphicsFamily(false), hasPresentFamily(false), hasComputeFamily(false) {}
			void SetGraphicsFamily(uint32_t queueFamily) { graphicsFamily = queueFamily; hasGraphicsFamily = true; }
			void SetPresentFamily(uint32_t queueFamily) { presentFamily = queueFamily; hasPresentFamily = true; }
			void SetComputeFamily(uint32_t queueFamily) { computeFamily = queueFamily; hasComputeFamily = true; }
		};

		/// @brief Vulkan Instance & Render(Physical/Logical) Device & VMAllocator Loader.
		class TinyVkDevice : public TinyDisposable {
		public:
			std::vector<const char*> validationLayers = { VK_VALIDATION_LAYER_KHRONOS_EXTENSION_NAME };
			std::vector<const char*> deviceExtensions = { VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME, VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME };
			std::vector<const char*> instanceExtensions = {  };

			const std::vector<VkPhysicalDeviceType> deviceTypes;
			VkPhysicalDeviceFeatures deviceFeatures {};
			const bool useComputeBit, useGraphicsBit, usePresentBit;
			
			VkInstance instance = VK_NULL_HANDLE;
			VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
			VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
			VkDevice logicalDevice = VK_NULL_HANDLE;
			VmaAllocator memoryAllocator = VK_NULL_HANDLE;
			TinyWindow* window = VK_NULL_HANDLE;
			VkSurfaceKHR presentSurface = VK_NULL_HANDLE;

			/// @brief Remove default copy destructor.
			TinyVkDevice(const TinyVkDevice&) = delete;
			
			/// @brief Remove default copy destructor.
			TinyVkDevice operator=(const TinyVkDevice&) = delete;

			/// @brief Calls the disposable interface dispose event.
			~TinyVkDevice() { this->Dispose(); }

			/// @brief Manually calls dispose on resources without deleting the object.
			void Disposable(bool waitIdle) {
				if (waitIdle) DeviceWaitIdle();

				#if TINY_ENGINE_VALIDATION
					DestroyDebugUtilsMessengerEXT(instance, debugMessenger, VK_NULL_HANDLE);
				#endif
				
				vmaDestroyAllocator(memoryAllocator);
				vkDestroyDevice(logicalDevice, VK_NULL_HANDLE);
				if (presentSurface != VK_NULL_HANDLE)
					vkDestroySurfaceKHR(instance, presentSurface, VK_NULL_HANDLE);
				vkDestroyInstance(instance, VK_NULL_HANDLE);
			}

			/// @brief Create managed VkDevice via Vulkan API. Initializes Vulkan: Must call Initialize() manually.
			TinyVkDevice(bool useGraphicsBit = true, bool useComputeBit = false, bool usePresentBit = false, TinyWindow* window = VK_NULL_HANDLE, const std::vector<VkPhysicalDeviceType> deviceTypes = { VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU }, VkPhysicalDeviceFeatures deviceFeatures = { .multiDrawIndirect = VK_TRUE })
			: window(window), deviceTypes(deviceTypes), useComputeBit(useComputeBit), useGraphicsBit(useGraphicsBit), usePresentBit(usePresentBit), deviceFeatures(deviceFeatures) {
				onDispose.hook(TinyCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));
				std::cout << "TESTING 1 2 3..." << std::endl;
			}

			/// @brief Queries device drivers for installed Validation Layers.
			bool QueryValidationLayerSupport() {
				uint32_t layersFound = 0, layersCount = 0;
				vkEnumerateInstanceLayerProperties(&layersCount, VK_NULL_HANDLE);
				std::vector<VkLayerProperties> availableLayers(layersCount);
				vkEnumerateInstanceLayerProperties(&layersCount, availableLayers.data());

				#if TINY_ENGINE_VALIDATION
					std::cout << "TinyEngine: Available Validation Layers:" << std::endl;
				#endif
				for (const auto& layerProperties : availableLayers) {
					#if TINY_ENGINE_VALIDATION
					std::cout << "\t" << layerProperties.layerName << std::endl;
					#endif

					for (const std::string layerName : validationLayers)
						if (!layerName.compare(layerProperties.layerName)) layersFound++;
				}

				return (layersFound == validationLayers.size());
			}

			/// @brief Wait for GPU device to finish transfer/render commands.
			VkResult DeviceWaitIdle() { return vkDeviceWaitIdle(logicalDevice); }

			/// @brief Returns info about the VkPhysicalDevice graphics/present queue families. If no surface provided, auto checks for Win32 surface support.
			TinyQueueFamily QueryPhysicalDeviceQueueFamilies(VkPhysicalDevice device = VK_NULL_HANDLE) {
				device = (device == VK_NULL_HANDLE)? physicalDevice : device;
				if (device == VK_NULL_HANDLE) return {};

				uint32_t queueFamilyCount = 0;
				vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, VK_NULL_HANDLE);
				std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
				vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
				
				int32_t useOutputBits = static_cast<int32_t>(useComputeBit) + static_cast<int32_t>(useGraphicsBit) + static_cast<int32_t>(usePresentBit);
				TinyQueueFamily indices = {};
				for (int i = 0; i < queueFamilies.size(); i++) {
					VkBool32 presentSupport = false;
					vkGetPhysicalDeviceSurfaceSupportKHR(device, i, presentSurface, &presentSupport);
					if (useComputeBit && !indices.computeFamily && queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { indices.SetComputeFamily(i); useOutputBits --; }
					if (useGraphicsBit && !indices.hasGraphicsFamily && queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { indices.SetGraphicsFamily(i); useOutputBits --; }
					if (usePresentBit && !indices.hasPresentFamily && presentSupport) { indices.SetPresentFamily(i); useOutputBits --; }
					if (useOutputBits <= 0) break;
				}
				return indices;
			}

			/// @brief Returns an VkDeviceSize ranking of VK_PHYSICAL_DEVICE_TYPE for a VkPhysicalDevice ranked by memory heap size.
			VkDeviceSize QueryPhysicalDeviceRankByHeapSize(VkPhysicalDevice device) {
				VkPhysicalDeviceMemoryProperties2 memoryProperties { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };
				vkGetPhysicalDeviceMemoryProperties2(device, &memoryProperties);
				for(VkMemoryHeap heap : memoryProperties.memoryProperties.memoryHeaps)
					if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
						return heap.size;
				return static_cast<VkDeviceSize>(0);
			}

			/// @brief Creates the underlying Vulkan Instance w/ Required Extensions.
			VkResult CreateVkInstance() {
				VkApplicationInfo appInfo {};
				appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
				appInfo.pApplicationName = window->hwndTitle.c_str();
				appInfo.applicationVersion = appInfo.engineVersion = appInfo.apiVersion = TINY_ENGINE_VERSION;
				appInfo.pEngineName = TINY_ENGINE_NAME;

				VkInstanceCreateInfo createInfo {};
				createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
				createInfo.pApplicationInfo = &appInfo;

				#if TINY_ENGINE_VALIDATION
					if (!QueryValidationLayerSupport()) {
						return VK_ERROR_VALIDATION_FAILED_EXT;
					} else {
						std::cout << "TinyEngine: Enabled Validation Layers:" << std::endl;
						for(const char* layer : validationLayers) std::cout << "\t" << layer << std::endl;
					}

					VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo { .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
					debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
					debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
					debugCreateInfo.pfnUserCallback = DebugCallback;
					debugCreateInfo.pUserData = VK_NULL_HANDLE;

					createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
					createInfo.ppEnabledLayerNames = validationLayers.data();
					createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
					
					instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
				#endif

				if (window != VK_NULL_HANDLE)
					for (const auto& extension : window->QueryRequiredExtensions())
						instanceExtensions.push_back(extension);
				
				createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
				createInfo.ppEnabledExtensionNames = instanceExtensions.data();

				VkResult result = vkCreateInstance(&createInfo, VK_NULL_HANDLE, &instance);
				if (result != VK_SUCCESS) {
					std::cout << "TinyEngine: Failed to create Vulkan instance! " << (VkResult) result << std::endl;
					return result;
				}

				#if TINY_ENGINE_VALIDATION
					result = CreateDebugUtilsMessengerEXT(instance, &debugCreateInfo, VK_NULL_HANDLE, &debugMessenger);
					if (result != VK_SUCCESS)
						return result;
					
					std::cout << "TinyEngine: " << instanceExtensions.size() << " instance extensions supported." << std::endl;
					for (const auto& extension : instanceExtensions) std::cout << '\t' << extension << std::endl;
				#endif
				return result;
			}

			/// @brief Returns the highest ranked GPU by device memory heap size (regardless of compatibility of settings).
			VkResult QueryPhysicalDevice() {
				uint32_t deviceCount = 0;
				vkEnumeratePhysicalDevices(instance, &deviceCount, VK_NULL_HANDLE);
				std::vector<VkPhysicalDevice> suitableDevices(deviceCount);
				vkEnumeratePhysicalDevices(instance, &deviceCount, suitableDevices.data());
				std::sort(suitableDevices.begin(), suitableDevices.end(), [this](VkPhysicalDevice A, VkPhysicalDevice B) {
					return QueryPhysicalDeviceRankByHeapSize(A) >= QueryPhysicalDeviceRankByHeapSize(B);
				});
				physicalDevice = (suitableDevices.size() > 0)? suitableDevices.front() : VK_NULL_HANDLE;
				
				#if TINY_ENGINE_VALIDATION
					if (physicalDevice != VK_NULL_HANDLE) {
						VkPhysicalDevicePushDescriptorPropertiesKHR pushDescriptorProps { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR };
						VkPhysicalDeviceProperties2 deviceProperties { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &pushDescriptorProps };
						vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties);
						TinyQueueFamily indices = QueryPhysicalDeviceQueueFamilies(physicalDevice);
						std::cout << "TinyEngine: GPU Hardware Info" << std::endl;
						std::cout << "\tGPU Device Name:         " << deviceProperties.properties.deviceName << std::endl;
						std::cout << "\tDevice Rank / Heap Size: " << QueryPhysicalDeviceRankByHeapSize(physicalDevice) << std::endl;
						std::cout << "\tPush Constant Memory:    " << deviceProperties.properties.limits.maxPushConstantsSize << " Bytes" << std::endl;
						std::cout << "\tPush Descriptor Memory:  " << pushDescriptorProps.maxPushDescriptors << " Count" << std::endl;
						std::cout << "\tPipelines:               Graphics = " << (indices.hasGraphicsFamily?"true":"false") << ", Compute = " << (indices.hasComputeFamily?"true":"false") << ", Present = " << (indices.hasPresentFamily?"true":"false") << "" << std::endl;
					}
				#endif

				if (physicalDevice == VK_NULL_HANDLE)
					return VK_ERROR_DEVICE_LOST;
				return VK_SUCCESS;
			}

			/// @brief Creates the logical devices for the graphics/present queue families.
			VkResult CreateLogicalDevice() {
				if (physicalDevice == VK_NULL_HANDLE)
					return VK_ERROR_DEVICE_LOST;

				std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
				TinyQueueFamily indices = QueryPhysicalDeviceQueueFamilies(physicalDevice);
				std::set<uint32_t> uniqueQueueFamilies = { indices.computeFamily, indices.graphicsFamily, indices.presentFamily };

				float queuePriority = 1.0f;
				for (uint32_t queueFamily : uniqueQueueFamilies) {
					VkDeviceQueueCreateInfo queueCreateInfo { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
					queueCreateInfo.queueFamilyIndex = queueFamily;
					queueCreateInfo.queueCount = 1;
					queueCreateInfo.pQueuePriorities = &queuePriority;
					queueCreateInfos.push_back(queueCreateInfo);
				}

				VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingCreateInfo { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR };
				dynamicRenderingCreateInfo.dynamicRendering = VK_TRUE;

				VkDeviceCreateInfo createInfo { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
				createInfo.pNext = &dynamicRenderingCreateInfo;
				createInfo.pQueueCreateInfos = queueCreateInfos.data();
				createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
				createInfo.pEnabledFeatures = &deviceFeatures;
				
				if (indices.hasPresentFamily) deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
				createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
				createInfo.ppEnabledExtensionNames = deviceExtensions.data();

				createInfo.enabledLayerCount = (TINY_ENGINE_VALIDATION)? static_cast<uint32_t>(validationLayers.size()) : 0;
				createInfo.ppEnabledLayerNames = (TINY_ENGINE_VALIDATION)? validationLayers.data() : VK_NULL_HANDLE;

				VkResult result = vkCreateDevice(physicalDevice, &createInfo, VK_NULL_HANDLE, &logicalDevice);

				#if TINY_ENGINE_VALIDATION
					if (result != VK_SUCCESS)
						std::cout << "TinyEngine: Failed to create logical device! Missing extension or queue family?" << std::endl;
				
					std::cout << "TinyEngine: " << deviceExtensions.size() << " device extensions supported." << std::endl;
					for (const auto& extension : deviceExtensions) std::cout << '\t' << extension << std::endl;
				#endif
				return result;
			}
			
			/// @brief Creates the VMAllocator for AMD's GPU memory handling API.
			VkResult CreateVMAllocator() {
				if (physicalDevice == VK_NULL_HANDLE)
					return VK_ERROR_INITIALIZATION_FAILED;
				VmaAllocatorCreateInfo allocatorCreateInfo { .vulkanApiVersion = TINY_ENGINE_VERSION, .physicalDevice = physicalDevice, .device = logicalDevice, .instance = instance };
				return vmaCreateAllocator(&allocatorCreateInfo, &memoryAllocator);
			}

			/// @brief Initializes the Vulkan Instance, Creates VMAllocator and Queries required Logical/Physical Device(s).
			VkResult Initialize() {
				VkResult result = CreateVkInstance();
				if (result != VK_SUCCESS) return result;
				result = vkCmdRenderingGetCallbacks(instance);
				if (result != VK_SUCCESS) return result;

				if (window != VK_NULL_HANDLE)
					presentSurface = window->CreateWindowSurface(instance);
				
				result = QueryPhysicalDevice();
				if (result != VK_SUCCESS) return result;
				result = CreateLogicalDevice();
				if (result != VK_SUCCESS) return result;
				return result = CreateVMAllocator();
			}

			/// @brief Constructor(...) + Initialize() with error result as combined TinyConstruct<Object,VkResult>.
			template<typename... A>
			inline static TinyConstruct<TinyVkDevice> Construct(bool useGraphicsBit = true, bool useComputeBit = false, bool usePresentBit = false, TinyWindow* window = VK_NULL_HANDLE, const std::vector<VkPhysicalDeviceType> deviceTypes = { VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU }, VkPhysicalDeviceFeatures deviceFeatures = { .multiDrawIndirect = VK_TRUE }) {
				std::unique_ptr<TinyVkDevice> object =
					std::make_unique<TinyVkDevice>(useGraphicsBit, useComputeBit, usePresentBit, window, deviceTypes, deviceFeatures);
				return TinyConstruct<TinyVkDevice>(object, object->Initialize());
			}			
		};
	}

#endif