#pragma once
#ifndef TINY_ENGINE_TINYUTILITIES
#define TINY_ENGINE_TINYUTILITIES
	#include "./TinyEngine.hpp"

	namespace TINY_ENGINE_NAMESPACE {
		#pragma region VULKAN_DEBUG_UTILITIES

		VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
			auto create = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
			if (create != VK_NULL_HANDLE)
				return create(instance, pCreateInfo, pAllocator, pDebugMessenger);
			return VK_ERROR_INITIALIZATION_FAILED;
		}

		VkResult DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
			auto destroy = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
			destroy(instance, debugMessenger, pAllocator);
			if (destroy != VK_NULL_HANDLE)
				return VK_SUCCESS;
			return VK_ERROR_INITIALIZATION_FAILED;
		}

		VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
			std::cout << "TinyEngine: Validation Layer: " << pCallbackData->pMessage << std::endl;
			return (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)? VK_TRUE : VK_FALSE;
		}

		#pragma endregion
		#pragma region VULKAN_DYNAMIC_RENDERING_FUNCTIONS

		PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingEXTKHR = VK_NULL_HANDLE;
		PFN_vkCmdEndRenderingKHR vkCmdEndRenderingEXTKHR = VK_NULL_HANDLE;
		PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetEXTKHR = VK_NULL_HANDLE;
		
		VkResult vkCmdRenderingGetCallbacks(VkInstance instance) {
			vkCmdBeginRenderingEXTKHR = (PFN_vkCmdBeginRenderingKHR)vkGetInstanceProcAddr(instance, "vkCmdBeginRenderingKHR");
			vkCmdEndRenderingEXTKHR = (PFN_vkCmdEndRenderingKHR)vkGetInstanceProcAddr(instance, "vkCmdEndRenderingKHR");
			vkCmdPushDescriptorSetEXTKHR = (PFN_vkCmdPushDescriptorSetKHR)vkGetInstanceProcAddr(instance, "vkCmdPushDescriptorSetKHR");
			
			if (vkCmdBeginRenderingEXTKHR == VK_NULL_HANDLE) return VK_ERROR_FEATURE_NOT_PRESENT;
			if (vkCmdEndRenderingEXTKHR == VK_NULL_HANDLE) return VK_ERROR_FEATURE_NOT_PRESENT;
			if (vkCmdPushDescriptorSetEXTKHR == VK_NULL_HANDLE) return VK_ERROR_FEATURE_NOT_PRESENT;
			return VK_SUCCESS;
		}

		VkResult vkCmdBeginRenderingEKHR(VkInstance instance, VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) {
			if (vkCmdBeginRenderingEXTKHR == VK_NULL_HANDLE) {
				#if TINY_ENGINE_VALIDATION
						std::cout << "TinyEngine: Failed to load VK_KHR_dynamic_rendering EXT function: PFN_vkCmdBeginRenderingKHR" << std::endl;
				#endif
				return VK_ERROR_INITIALIZATION_FAILED;
			}

			vkCmdBeginRenderingEXTKHR(commandBuffer, pRenderingInfo);
			return VK_SUCCESS;
		}

		VkResult vkCmdEndRenderingEKHR(VkInstance instance, VkCommandBuffer commandBuffer) {
			if (vkCmdEndRenderingEXTKHR == VK_NULL_HANDLE) {
				#if TINY_ENGINE_VALIDATION
						std::cout << "TinyEngine: Failed to load VK_KHR_dynamic_rendering EXT function: PFN_vkCmdEndRenderingKHR" << std::endl;
				#endif
				return VK_ERROR_INITIALIZATION_FAILED;
			}

			vkCmdEndRenderingEXTKHR(commandBuffer);
			return VK_SUCCESS;
		}

		VkResult vkCmdPushDescriptorSetEKHR(VkInstance instance, VkCommandBuffer commandBuffer, VkPipelineBindPoint bindPoint, VkPipelineLayout layout, uint32_t set, uint32_t writeCount, const VkWriteDescriptorSet* pWriteSets) {
			if (vkCmdPushDescriptorSetEXTKHR == VK_NULL_HANDLE) {
				#if TINY_ENGINE_VALIDATION
					std::cout << "TinyEngine: Failed to load VK_KHR_dynamic_rendering EXT function: PFN_vkCmdPushDescriptorSetKHR" << std::endl;
				#endif
				return VK_ERROR_INITIALIZATION_FAILED;
			}

			vkCmdPushDescriptorSetEXTKHR(commandBuffer, bindPoint, layout, set, writeCount, pWriteSets);
			return VK_SUCCESS;
		}

		#pragma endregion
        #pragma region VULKAN_INTERFACE SUPPORT

		/// @brief Description of the SwapChain Rendering format.
		struct TinySwapChainSupporter {
		public:
			VkSurfaceCapabilitiesKHR capabilities = {};
			std::vector<VkSurfaceFormatKHR> formats = {};
			std::vector<VkPresentModeKHR> presentModes = {};
		};

		/// @brief Description of the Rendering Surface format.
		struct TinySurfaceSupporter {
		public:
			VkFormat dataFormat = VK_FORMAT_B8G8R8A8_UNORM;
			VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
			VkPresentModeKHR idealPresentMode = VK_PRESENT_MODE_FIFO_KHR;
		};

        #pragma endregion
		#pragma region VULKAN_ENUMERATE_HELPER_FUNCTIONS

		VkResult QueryPhysicalDevices(VkInstance instance, std::vector<VkPhysicalDevice>& devices) {
			uint32_t deviceCount;
			VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, VK_NULL_HANDLE);
			devices.resize(deviceCount);
			if (result == VK_SUCCESS)
				result = vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
			return result;
		}

		VkResult QueryQueueFamilyProperties(VkPhysicalDevice pdevice, std::vector<VkQueueFamilyProperties>& queueFamilies) {
			uint32_t queueFamilyCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &queueFamilyCount, VK_NULL_HANDLE);
			queueFamilies.resize(queueFamilyCount);
			if (queueFamilyCount > 0) {
				vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &queueFamilyCount, queueFamilies.data());
				return VK_SUCCESS;
			}
			return VK_ERROR_DEVICE_LOST;
		}

		#pragma endregion
		#pragma region VULKAN_DEFAULT_PIPELINE_STATES
		
		const VkDebugUtilsMessengerCreateInfoEXT defaultDebugCreateInfo {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType = /*VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |*/ VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback = DebugCallback,
			.pUserData = VK_NULL_HANDLE
		};

		const VkApplicationInfo defaultAppInfo {
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName = TINY_ENGINE_NAME,
			.applicationVersion = TINY_ENGINE_VERSION,
			.engineVersion = TINY_ENGINE_VERSION,
			.apiVersion = TINY_ENGINE_VERSION,
			.pEngineName = TINY_ENGINE_NAME
		};
		
		const VkPhysicalDeviceTimelineSemaphoreFeatures defaultTimelineSemaphoreFeatures {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES,
			.timelineSemaphore = VK_TRUE
		};

		const VkPhysicalDevicePushDescriptorPropertiesKHR defaultPushDescriptorProperties {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR
		};

		const VkPhysicalDeviceDynamicRenderingFeatures defaultDynamicRenderingCreateInfo {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
			.dynamicRendering = VK_TRUE,
			.pNext = const_cast<VkPhysicalDeviceTimelineSemaphoreFeatures*>(&defaultTimelineSemaphoreFeatures)
		};

		const VkPipelineVertexInputStateCreateInfo defaultVertexInputInfo {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.pVertexBindingDescriptions = VK_NULL_HANDLE,
			.vertexBindingDescriptionCount = 0,
			.pVertexAttributeDescriptions = VK_NULL_HANDLE,
			.vertexAttributeDescriptionCount = 0
		};
		
		const VkPipelineInputAssemblyStateCreateInfo defaultInputAssembly {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = {},
			.primitiveRestartEnable = VK_FALSE
		};

		const VkPipelineViewportStateCreateInfo defaultViewportState {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.scissorCount = 1,
			.flags = 0
		};

		const VkPipelineRasterizationStateCreateInfo defaultRasterizer {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.depthBiasEnable = VK_FALSE,
			.depthClampEnable = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode = {},
			.lineWidth = 1.0f,
			.cullMode = VK_CULL_MODE_BACK_BIT,
			.frontFace = VK_FRONT_FACE_CLOCKWISE
		};
		
		const VkPipelineMultisampleStateCreateInfo defaultMultisampling {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
			.sampleShadingEnable = VK_FALSE
		};
		
		const VkPipelineColorBlendAttachmentState defaultColorBlendState = {
			.blendEnable = VK_TRUE,
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
			.colorBlendOp = VK_BLEND_OP_ADD,
			.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.alphaBlendOp = VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
		};

		const VkPipelineColorBlendStateCreateInfo defaultColorBlending {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.logicOpEnable = VK_FALSE,
			.logicOp = VK_LOGIC_OP_COPY,
			.attachmentCount = 1,
			.pAttachments = VK_NULL_HANDLE,
			.blendConstants[0] = 0.0f,
			.blendConstants[1] = 0.0f,
			.blendConstants[2] = 0.0f,
			.blendConstants[3] = 0.0f
		};
		
		const std::array<VkDynamicState, 2> defaultDynamicStateEnables = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		const VkPipelineDynamicStateCreateInfo defaultDynamicState {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.pDynamicStates = defaultDynamicStateEnables.data(),
			.dynamicStateCount = static_cast<uint32_t>(defaultDynamicStateEnables.size()),
			.flags = 0,
			.pNext = VK_NULL_HANDLE
		};
		
		const VkPipelineRenderingCreateInfoKHR defaultRenderingCreateInfo {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
			.pColorAttachmentFormats = VK_NULL_HANDLE,
			.depthAttachmentFormat = {},
			.colorAttachmentCount = 1
		};

		const VkPipelineDepthStencilStateCreateInfo defaultDepthStencilInfo {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = VK_FALSE,
			.depthWriteEnable = VK_FALSE,
			.depthCompareOp = VK_COMPARE_OP_LESS,
			.depthBoundsTestEnable = VK_FALSE,
			.minDepthBounds = 0.0f,
			.maxDepthBounds = 1.0f,
			.stencilTestEnable = VK_FALSE,
			.front = {}, .back = {}
		};
		
		#pragma endregion
	}
#endif