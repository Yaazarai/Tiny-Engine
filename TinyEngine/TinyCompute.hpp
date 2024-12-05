#pragma once
#ifndef TINy_ENGINE_TINYCOMPUTE
#define TINy_ENGINE_TINYCOMPUTE
	#include "./TinyEngine.hpp"

	namespace TINY_ENGINE_NAMESPACE {
		/// @brief Vulkan Compute Pipeline & Renderer using Storage Buffers/Images and Push Descriptors/Constants.
		class TinyCompute : public TinyDisposable {
        public:
            TinyVkDevice& vkdevice;
			TinyCommandPool& commandPool;

            const std::string shader;
			VkDescriptorSetLayout descriptorLayout;
			std::vector<VkDescriptorSetLayoutBinding> descriptorBindings;
			std::vector<VkPushConstantRange> pushConstantRanges;

			VkPipelineLayout computePipelineLayout;
			VkPipeline computePipeline;
			VkQueue computeQueue;
			uint32_t maxWorkGroups[3], maxSizeOfWorkGroups[3];

            /// Invokable Render Events: (executed in TinyCompute::RenderExecute()
			TinyInvokable<TinyCommandPool&> onRenderEvents;

			/// @brief Remove default copy destructor.
			TinyCompute(const TinyCompute&) = delete;
			
			/// @brief Remove default copy destructor.
			TinyCompute operator=(const TinyCompute&) = delete;

			/// @brief Calls the disposable interface dispose event.
			~TinyCompute() { this->Dispose(); }

			/// @brief Manually calls dispose on resources without deleting the object.
			void Disposable(bool waitIdle) {
				if (waitIdle) vkdevice.DeviceWaitIdle();

				vkDestroyDescriptorSetLayout(vkdevice.logicalDevice, descriptorLayout, VK_NULL_HANDLE);
				vkDestroyPipeline(vkdevice.logicalDevice, computePipeline, VK_NULL_HANDLE);
				vkDestroyPipelineLayout(vkdevice.logicalDevice, computePipelineLayout, VK_NULL_HANDLE);
			}
            
			/// @brief Creates a managed Compute Pipeline and dispatcher for performing compute commands without a graphics pipeline.
			TinyCompute(TinyVkDevice& vkdevice, TinyCommandPool& commandPool, const std::string shader, const std::vector<VkDescriptorSetLayoutBinding>& descriptorBindings, const std::vector<VkPushConstantRange>& pushConstantRanges)
            : vkdevice(vkdevice), commandPool(commandPool), shader(shader) {
				onDispose.hook(TinyCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));
				TinyQueueFamily indices = vkdevice.QueryPhysicalDeviceQueueFamilies();
				vkGetDeviceQueue(vkdevice.logicalDevice, indices.computeFamily, 0, &computeQueue);
            }

			/// @brief Read out a SPIR-V shader file.
			std::vector<char> ReadShaderFile(const std::string& path) {
				std::ifstream file(path, std::ios::ate | std::ios::binary);
				if (file.is_open()) {
					size_t fsize = static_cast<size_t>(file.tellg());
					std::vector<char> buffer(fsize);
					file.seekg(0);
					file.read(buffer.data(), fsize);
					file.close();
					return buffer;
				}
				return {};
			}
			
			/// @brief Create a shader module of an imported SPIR-V shader file.
			VkShaderModule CreateShaderModule(std::vector<char> shaderCode) {
				VkShaderModuleCreateInfo createInfo {
					.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
					.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data()), .codeSize = shaderCode.size(), .flags = 0, .pNext = VK_NULL_HANDLE,
				};

				VkShaderModule shaderModule;
				if (vkCreateShaderModule(vkdevice.logicalDevice, &createInfo, VK_NULL_HANDLE, &shaderModule) != VK_SUCCESS)
					return VK_NULL_HANDLE;

				return shaderModule;
			}
			
            /// @brief Create compute pipeline load shader(s).
			VkResult CreateComputePipeline() {
				TinyQueueFamily indices = vkdevice.QueryPhysicalDeviceQueueFamilies();
				if (!indices.hasComputeFamily)
					return VK_ERROR_INITIALIZATION_FAILED;
				
				VkShaderModule shaderModule;
				auto shaderCode = ReadShaderFile(shader);
				shaderModule = CreateShaderModule(shaderCode);

                if (shaderModule == VK_NULL_HANDLE)
                    return VK_ERROR_INITIALIZATION_FAILED;
                
				#if TINY_ENGINE_VALIDATION
				std::cout << "TinyVulkan: Loading Shader @ " << shader << std::endl;
				#endif
				VkPipelineShaderStageCreateInfo shaderPipelineCreateInfo = {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = shaderModule, .pName = "main"
				};

				///////////////////////////////////////////////////////////////////////////////////////////////////////
				///////////////////////////////////////////////////////////////////////////////////////////////////////
				VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
				pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
				
				pipelineLayoutInfo.pushConstantRangeCount = 0;
				uint32_t pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
				if (pushConstantRangeCount > 0) {
					pipelineLayoutInfo.pushConstantRangeCount = pushConstantRangeCount;
					pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();
				}

				VkResult result = VK_SUCCESS;
				if (descriptorBindings.size() > 0) {
					VkDescriptorSetLayoutCreateInfo descriptorCreateInfo{};
					descriptorCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
					descriptorCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
					descriptorCreateInfo.bindingCount = static_cast<uint32_t>(descriptorBindings.size());
					descriptorCreateInfo.pBindings = descriptorBindings.data();

					result = vkCreateDescriptorSetLayout(vkdevice.logicalDevice, &descriptorCreateInfo, VK_NULL_HANDLE, &descriptorLayout);
					if (result != VK_SUCCESS) return result;

					pipelineLayoutInfo.setLayoutCount = 1;
					pipelineLayoutInfo.pSetLayouts = &descriptorLayout;
				}

				result = vkCreatePipelineLayout(vkdevice.logicalDevice, &pipelineLayoutInfo, VK_NULL_HANDLE, &computePipelineLayout);
				if (result != VK_SUCCESS) return result;
				///////////////////////////////////////////////////////////////////////////////////////////////////////
				///////////////////////////////////////////////////////////////////////////////////////////////////////

                VkComputePipelineCreateInfo pipelineInfo;
                pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
                pipelineInfo.flags = VK_PIPELINE_CREATE_COLOR_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT;
                pipelineInfo.stage = shaderPipelineCreateInfo;
                pipelineInfo.layout = computePipelineLayout;
                pipelineInfo.basePipelineHandle;
                pipelineInfo.basePipelineIndex;
                
                result = vkCreateComputePipelines(vkdevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, VK_NULL_HANDLE, &computePipeline);
                vkDestroyShaderModule(vkdevice.logicalDevice, shaderModule, VK_NULL_HANDLE);

				VkPhysicalDeviceLimits deviceLimits {};
				VkPhysicalDeviceProperties2 properties {};
				properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
				vkGetPhysicalDeviceProperties2(vkdevice.physicalDevice, &properties);

				maxWorkGroups[0] = properties.properties.limits.maxComputeWorkGroupCount[0];
				maxWorkGroups[2] = properties.properties.limits.maxComputeWorkGroupCount[1];
				maxWorkGroups[3] = properties.properties.limits.maxComputeWorkGroupCount[2];
				maxSizeOfWorkGroups[0] = properties.properties.limits.maxComputeWorkGroupSize[0];
				maxSizeOfWorkGroups[2] = properties.properties.limits.maxComputeWorkGroupSize[1];
				maxSizeOfWorkGroups[3] = properties.properties.limits.maxComputeWorkGroupSize[2];
                return result;
            }

			/// @brief Begins recording render commands to the provided command buffer.
			VkResult BeginRecordCmdBuffer(VkCommandBuffer commandBuffer, std::vector<TinyBuffer*> syncStorageBuffers, std::vector<TinyImage*> syncStorageImages, const VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f }, const VkClearValue depthStencil = { .depthStencil = { 1.0f, 0 } }) {
				VkCommandBufferBeginInfo beginInfo{};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
				beginInfo.pInheritanceInfo = VK_NULL_HANDLE; // Optional

				VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
				if (result != VK_SUCCESS) return result;
				
				for(TinyBuffer* buffer : syncStorageBuffers)
					buffer->MemoryPipelineBarrier(commandBuffer, TinyCmdBufferSubmitStage::STAGE_BEGIN);
				
				for(TinyImage* image : syncStorageImages)
					image->TransitionLayoutBarrier(commandBuffer, TinyCmdBufferSubmitStage::STAGE_BEGIN, TinyImageLayout::LAYOUT_GENERAL);

				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
                return VK_SUCCESS;
			}

			/// @brief Ends recording render commands to the provided command buffer.
			VkResult EndRecordCmdBuffer(VkCommandBuffer commandBuffer, std::vector<TinyBuffer*> syncStorageBuffers, std::vector<TinyImage*> syncStorageImages, const VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f }, const VkClearValue depthStencil = { .depthStencil = { 1.0f, 0 } }) {
				for(TinyBuffer* buffer : syncStorageBuffers)
					buffer->MemoryPipelineBarrier(commandBuffer, TinyCmdBufferSubmitStage::STAGE_END);
				
				for(TinyImage* image : syncStorageImages)
					image->TransitionLayoutBarrier(commandBuffer, TinyCmdBufferSubmitStage::STAGE_END, TinyImageLayout::LAYOUT_GENERAL);
				
				VkResult result = vkEndCommandBuffer(commandBuffer);
                return result;
			}

			/// @brief Records Push Descriptors to the command buffer.
			VkResult PushDescriptorSet(VkCommandBuffer cmdBuffer, std::vector<VkWriteDescriptorSet> writeDescriptorSets) {
				return vkCmdPushDescriptorSetEKHR(vkdevice.instance, cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data());
			}

			/// @brief Records Push Constants to the command buffer.
			void PushConstants(VkCommandBuffer cmdBuffer, uint32_t byteSize, const void* pValues) {
				vkCmdPushConstants(cmdBuffer, computePipelineLayout, VK_PIPELINE_BIND_POINT_COMPUTE, 0, byteSize, pValues);
			}

			/// @brief Dispatches X*Y*Z number of work-groups with the size of each work-group specified within the compute shader.
			VkResult CmdispatchGroups(VkCommandBuffer commandBuffer, std::array<uint32_t,3> wgroups, std::array<uint32_t,3> basewg) {
				if (wgroups[0] > maxWorkGroups[0] || wgroups[1] > maxWorkGroups[1] || wgroups[2] > maxWorkGroups[2]) {
					#if TINY_ENGINE_VALIDATION
					std::cerr << "TinyVulkan: Tried to Dispatch [" << wgroups[0] << ", " << wgroups[1] << ", " << wgroups[2] << "]";
					std::cerr << " Work Groups, however device limits are: " << maxWorkGroups[0] << "," << maxWorkGroups[1] << "," << maxWorkGroups[2];
                    #endif
					return VK_ERROR_TOO_MANY_OBJECTS;
				}
				
				vkCmdDispatchBase(commandBuffer, wgroups[0], wgroups[1], wgroups[2], basewg[0], basewg[1], basewg[2]);
                return VK_SUCCESS;
			}

			/// @brief Executes the registered onRenderEvents and renders them to the target storage buffer.
			VkResult ComputeExecute(bool waitFences = true, std::vector<TinyBuffer*> storageBuffers = {}, std::vector<TinyImage*> storageImages = {}) {
				std::vector<VkFence> fences;
				if (waitFences) {
					for(TinyBuffer* buffer : storageBuffers)
						fences.push_back(buffer->bufferWaitable);
					
					for(TinyImage* image : storageImages)
						fences.push_back(image->imageWaitable);
					
					vkWaitForFences(vkdevice.logicalDevice, fences.size(), fences.data(), VK_TRUE, UINT64_MAX);
				}
				
				//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
				commandPool.ReturnAllBuffers();
                onRenderEvents.invoke(commandPool);
				//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                
				std::vector<VkCommandBuffer> commandBuffers;
				auto buffers = commandPool.commandBuffers;
				std::for_each(buffers.begin(), buffers.end(),
					[&commandBuffers](std::pair<VkCommandBuffer, VkBool32> cmdBuffer){
						if (cmdBuffer.second) commandBuffers.push_back(cmdBuffer.first);
				});

				VkSubmitInfo submitInfo{};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
				submitInfo.pCommandBuffers = commandBuffers.data();
				return vkQueueSubmit(computeQueue, 1, &submitInfo, fences[0]);
			}

			// Initialize Compute pipeline and dispatcher.
			VkResult Initialize() {
                return CreateComputePipeline();
            }

			/// @brief Constructor(...) + Initialize() with error result as combined TinyObject<Object,VkResult>.
			template<typename... A>
			inline static TinyObject<TinyCompute> Construct(TinyVkDevice& vkdevice, TinyCommandPool& commandPool, const std::string shader, const std::vector<VkDescriptorSetLayoutBinding>& descriptorBindings, const std::vector<VkPushConstantRange>& pushConstantRanges) {
				std::unique_ptr<TinyCompute> object =
					std::make_unique<TinyCompute>(vkdevice, commandPool, shader, descriptorBindings, pushConstantRanges);
				return TinyObject<TinyCompute>(object, object->Initialize());
			}
        };
    }
#endif

///
///	Rendering with Compute:
///		1. Pass storage image with: (tells the shader we can read/write to an image)
///			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
///			sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
///		2. Transition image layout to VK_IMAGE_LAYOUT_GENERAL via memory pipeline barrier (synchronization).
///		3. Wait on used storage images/buffers fences (synchronization).
///			* Not required if they are not being modified by other threads/queues.
///		3. Dispatch compute commands into command buffer executed with image layout transition (if needed).
///		4. Submit command buffer work to the compute pipeline's compute queue.
///	
///	Dispatching Groups:
///		Compute Shaders dispatch threads, each thread performs some amount of work.
///		Threads are dispatched in groups called "Work Groups."
///		The size of each Work Group is defined within the compute shader (constant).
///		The number of Work Groups is what we dispatch to perform said work.
///		This makes it difficult to spawn an exact size Work Group unless the group is very large.
///		However large Work Groups may not fully occupy the GPU and may be sub-optimal.
///		Profile as needed for optimal performance.
///