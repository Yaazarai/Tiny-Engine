#pragma once
#ifndef TINY_ENGINE_TINYUTILITIES
#define TINY_ENGINE_TINYUTILITIES
	#include "./TinyEngine.hpp"

	namespace TINY_ENGINE_NAMESPACE {
		#pragma region VULKAN_DEBUG_UTILITIES
		
		template <typename T>
		class TinyConstruct {
		public:
			std::unique_ptr<T> source;
			VkResult result;
			
			TinyConstruct(const TinyConstruct&) = delete;
			TinyConstruct operator=(TinyConstruct&) = delete;
			TinyConstruct(std::unique_ptr<T>& source, VkResult result) : source(std::move(source)), result(result) {}

			operator T&() { return static_cast<T&>(*source.get()); }
			operator T*() { return static_cast<T*>(source.get()); }
			T& ref() { return *source.get(); }
			T* ptr() { return source.get(); }
		};
		
		class TinyRuntimeError : public std::runtime_error {
			public:
			VkResult result = VK_SUCCESS;
			std::string message;

			explicit TinyRuntimeError(VkResult result, std::string _Message) : result(result), message(_Message), runtime_error(_Message) {
				#if TINY_ENGINE_VALIDATION
					std::cout << "[runtime error = " << static_cast<int32_t>(result) << "] : " << _Message << std::endl;
				#endif
			}

			explicit TinyRuntimeError(VkResult result, const char* _Message) : result(result), message(_Message), runtime_error(_Message) {
				#if TINY_ENGINE_VALIDATION
					std::cout << "[runtime error = " << static_cast<int32_t>(result) << "] : " << _Message << std::endl;
				#endif
			}

			operator int32_t() { return static_cast<int32_t>(result); }

			operator std::string() { return message; }
		};

		VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
			auto create = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
			if (create != VK_NULL_HANDLE)
				return create(instance, pCreateInfo, pAllocator, pDebugMessenger);
			return VK_ERROR_INITIALIZATION_FAILED;
		}

		VkResult DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
			auto destroy = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
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

		class TinyRenderInterface {
		public:
			/// @brief Alias call for easy-calls to: vkCmdBindVertexBuffers + vkCmdBindIndexBuffer.
			inline static void CmdBindGeometry(VkCommandBuffer cmdBuffer, const VkBuffer* vertexBuffers, const VkBuffer indexBuffer, const VkDeviceSize* offsets, const VkDeviceSize indexOffset = 0, uint32_t binding = 0, uint32_t bindingCount = 1) {
				vkCmdBindVertexBuffers(cmdBuffer, binding, bindingCount, vertexBuffers, offsets);
				vkCmdBindIndexBuffer(cmdBuffer, indexBuffer, indexOffset, VK_INDEX_TYPE_UINT32);
			}

			/// @brief Alias call for: vkCmdBindVertexBuffers.
			inline static void CmdBindGeometry(VkCommandBuffer cmdBuffer, const VkBuffer* vertexBuffers, const VkDeviceSize* offsets, uint32_t binding = 0, uint32_t bindingCount = 1) {
				vkCmdBindVertexBuffers(cmdBuffer, binding, bindingCount, vertexBuffers, offsets);
			}

			/// @brief Alias call for: vkCmdBindIndexBuffers.
			inline static void CmdBindGeometry(VkCommandBuffer cmdBuffer, const VkBuffer indexBuffer, const VkDeviceSize indexOffset = 0, uint32_t binding = 0, uint32_t bindingCount = 1) {
				vkCmdBindIndexBuffer(cmdBuffer, indexBuffer, indexOffset, VK_INDEX_TYPE_UINT32);
			}

			/// @brief Alias call for: vkCmdBindVertexBuffers2.
			inline static void CmdBindGeometry(VkCommandBuffer cmdBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* vertexBuffers, const VkDeviceSize* vbufferOffsets, const VkDeviceSize* vbufferSizes, const VkDeviceSize* vbufferStrides = VK_NULL_HANDLE) {
				vkCmdBindVertexBuffers2(cmdBuffer, firstBinding, bindingCount, vertexBuffers, vbufferOffsets, vbufferSizes, vbufferStrides);
			}

			/// @brief Alias call for vkCmdDraw (isIndexed = false) and vkCmdDrawIndexed (isIndexed = true).
			inline static void CmdDrawGeometry(VkCommandBuffer cmdBuffer, bool isIndexed = false, uint32_t instanceCount = 1, uint32_t firstInstance = 0, uint32_t vertexCount = 0, uint32_t vertexOffset = 0, uint32_t firstIndex = 0) {
				switch (isIndexed) {
					case true:
					vkCmdDrawIndexed(cmdBuffer, vertexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
					break;
					case false:
					vkCmdDraw(cmdBuffer, vertexCount, instanceCount, vertexOffset, firstInstance);
					break;
				}
			}

			/// @brief Alias call for: vkCmdDrawIndexedIndirect.
			inline static void CmdDrawGeometryIndirect(VkCommandBuffer cmdBuffer, const VkBuffer drawParamBuffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
				vkCmdDrawIndexedIndirect(cmdBuffer, drawParamBuffer, offset, drawCount, stride);
			}

			/// @brief Alias call for: vkCmdDrawIndexedIndirectCount.
			inline static void CmdDrawGeometryIndirect(VkCommandBuffer cmdBuffer, const VkBuffer drawParamBuffer, VkDeviceSize offset, const VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t drawCount, uint32_t maxDrawCount, uint32_t stride) {
				vkCmdDrawIndexedIndirectCount(cmdBuffer, drawParamBuffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
			}
		};

        #pragma endregion
	}
#endif