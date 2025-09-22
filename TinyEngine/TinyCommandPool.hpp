#pragma once
#ifndef TINY_ENGINE_TINYCOMMANDPOOL
#define TINY_ENGINE_TINYCOMMANDPOOL
	#include "./TinyEngine.hpp"

	namespace TINY_ENGINE_NAMESPACE {
		/// @brief Pool of managed rentable VkCommandBuffers for performing rendering/transfer operations.
		class TinyCommandPool : public TinyDisposable {
		public:
			TinyVkDevice& vkdevice;
			VkCommandPool commandPool;
			size_t bufferCount;
			std::vector<std::pair<VkCommandBuffer, VkBool32>> commandBuffers;
			VkResult initialized = VK_ERROR_INITIALIZATION_FAILED;

			/// @brief Remove default copy destructor.
			TinyCommandPool(const TinyCommandPool&) = delete;
            
			/// @brief Remove default copy destructor.
			TinyCommandPool operator=(const TinyCommandPool&) = delete;
			
			/// @brief Calls the disposable interface dispose event.
			~TinyCommandPool() { this->Dispose(); }

			/// @brief Manually calls dispose on resources without deleting the object.
			void Disposable(bool waitIdle) {
				if (waitIdle) vkdevice.DeviceWaitIdle();
				vkDestroyCommandPool(vkdevice.logicalDevice, commandPool, VK_NULL_HANDLE);
			}
			
			/// @brief Creates a command pool to lease VkCommandBuffers from for recording render commands.
			TinyCommandPool(TinyVkDevice& vkdevice, size_t bufferCount = 32UL) : vkdevice(vkdevice), bufferCount(bufferCount) {
				onDispose.hook(TinyCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));
				initialized = Initialize();
			}

			/// @brief Creates the underlying command pool with: VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT enabled.
			VkResult CreateCommandPool() {
				VkCommandPoolCreateInfo poolInfo { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT };
				TinyQueueFamily queueFamily = vkdevice.QueryPhysicalDeviceQueueFamilies();
				
				if (queueFamily.hasGraphicsFamily)
					poolInfo.queueFamilyIndex = queueFamily.graphicsFamily;

				VkResult result = (!queueFamily.hasGraphicsFamily)? VK_ERROR_INITIALIZATION_FAILED : VK_SUCCESS;
				if (result == VK_SUCCESS)
					result = vkCreateCommandPool(vkdevice.logicalDevice, &poolInfo, VK_NULL_HANDLE, &commandPool);
                return result;
			}

			/// @brief Allocates CommandBuffers to host/user device memory with: VK_COMMAND_BUFFER_LEVEL_PRIMARY enabled.
			VkResult CreateCommandBuffers(size_t bufferCount = 1) {
				VkCommandBufferAllocateInfo allocInfo {
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
					.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
					.commandPool = commandPool,
					.commandBufferCount = static_cast<uint32_t>(bufferCount)
				};
				
                std::vector<VkCommandBuffer> temporary(bufferCount);
				VkResult result = vkAllocateCommandBuffers(vkdevice.logicalDevice, &allocInfo, temporary.data());

				if (result == VK_SUCCESS)
					std::for_each(temporary.begin(), temporary.end(), [&buffers = commandBuffers](VkCommandBuffer cmdBuffer) { buffers.push_back(std::pair(cmdBuffer, static_cast<VkBool32>(false))); });
                return result;
			}
            
            /// @brief Returns true/false if ANY VkCommandBuffers are available to be Leased.
			bool HasBuffers() {
				for (auto& cmdBuffer : commandBuffers)
					if (!cmdBuffer.second) return true;
				return false;
			}

			/// @brief Returns the number of available VkCommandBuffers that can be Leased.
			size_t HasBuffersCount() {
				size_t count = 0;
				for(auto& cmdBuffer : commandBuffers)
					count += static_cast<size_t>(!cmdBuffer.second);
				return count;
			}

			/// @brief Reserves a VkCommandBuffer for use and returns the VkCommandBuffer and it's ID (used for returning to the pool).
			std::pair<VkCommandBuffer,int32_t> LeaseBuffer(bool resetCmdBuffer = false) {
				size_t index = 0;
				for(auto& cmdBuffer : commandBuffers)
					if (!cmdBuffer.second) {
						cmdBuffer.second = true;
						if (resetCmdBuffer)
                            vkResetCommandBuffer(cmdBuffer.first, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
						return std::pair(cmdBuffer.first, index++);
					}
				return std::pair<VkCommandBuffer,int32_t>(VK_NULL_HANDLE,-1);
			}

			/// @brief Free's up the VkCommandBuffer that was previously rented for re-use.
			VkResult ReturnBuffer(std::pair<VkCommandBuffer, int32_t> bufferIndexPair) {
				if (bufferIndexPair.second < 0 || bufferIndexPair .second >= commandBuffers.size())
					return VK_ERROR_NOT_PERMITTED_KHR;

				commandBuffers[bufferIndexPair.second].second = false;
                return VK_SUCCESS;
			}

			/// @brief Sets all of the command buffers to available--optionally resets their recorded commands.
			VkResult ReturnAllBuffers() {
				VkResult result = vkResetCommandPool(vkdevice.logicalDevice, commandPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
				if (result == VK_SUCCESS) for(auto& cmdBuffer : commandBuffers) cmdBuffer.second = false;
                return result;
			}

			/// @brief Creates the CommandPool & CommandBuffers for submitting rendering commands.
			VkResult Initialize() {
                VkResult result = CreateCommandPool();
                if (result != VK_SUCCESS) return result;
                return CreateCommandBuffers(bufferCount);
            }
		};
	}
#endif