#pragma once
#ifndef TINY_ENGINE_TINYGRAPHICSPIPELINE
#define TINY_ENGINE_TINYGRAPHICSPIPELINE
	#include "./TinyEngine.hpp"

	namespace TINY_ENGINE_NAMESPACE {
		#define VKCOMP_RGBA_BITS VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		#define VKCOMP_BGRA_BITS VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_A_BIT

		/// @brief Represents the Vertex shader layout data passing through the graphics pipeline.
		struct TinyVertexDescription {
			const VkVertexInputBindingDescription binding;
			const std::vector<VkVertexInputAttributeDescription> attributes;

			TinyVertexDescription(VkVertexInputBindingDescription binding, const std::vector<VkVertexInputAttributeDescription> attributes) : binding(binding), attributes(attributes) {}
		};

		/// @brief Vulkan Graphics Pipeline using Dynamic Viewports/Scissors, Push Descriptors/Constants.
		class TinyGraphicsPipeline : public TinyDisposable {
		public:
			TinyVkDevice& vkdevice;
			
			const std::vector<std::tuple<TinyShaderStages, std::string>> shaders;
			std::vector<VkDescriptorSetLayoutBinding> descriptorBindings;
			std::vector<VkPushConstantRange> pushConstantRanges;
			VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;

			VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
			VkPipeline graphicsPipeline = VK_NULL_HANDLE;
			VkQueue graphicsQueue;
			VkQueue presentQueue;
			
			VkFormat imageFormat;
			VkPipelineColorBlendAttachmentState colorBlendState;
			VkColorComponentFlags colorComponentFlags;
			
			TinyVertexDescription vertexDescription;
			VkPrimitiveTopology vertexTopology;
			VkPolygonMode polgyonTopology;
			
			bool enableBlending;
			bool enableDepthTesting;

			/// @brief Remove default copy destructor.
			TinyGraphicsPipeline(const TinyGraphicsPipeline&) = delete;
            
			/// @brief Remove default copy destructor.
			TinyGraphicsPipeline operator=(const TinyGraphicsPipeline&) = delete;
			
			/// @brief Calls the disposable interface dispose event.
			~TinyGraphicsPipeline() { this->Dispose(); }

			/// @brief Manually calls dispose on resources without deleting the object.
			void Disposable(bool waitIdle) {
				if (waitIdle) vkdevice.DeviceWaitIdle();

				if (descriptorLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(vkdevice.logicalDevice, descriptorLayout, VK_NULL_HANDLE);
				if (graphicsPipeline != VK_NULL_HANDLE) vkDestroyPipeline(vkdevice.logicalDevice, graphicsPipeline, VK_NULL_HANDLE);
				if (pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(vkdevice.logicalDevice, pipelineLayout, VK_NULL_HANDLE);
			}

			/// @brief Creates a Managed VkPipeline which defines our raster graphics stages: Must call Initialize() manually.
			TinyGraphicsPipeline(TinyVkDevice& vkdevice, TinyVertexDescription vertexDescription, const std::vector<std::tuple<TinyShaderStages, std::string>> shaders, const std::vector<VkDescriptorSetLayoutBinding>& descriptorBindings, const std::vector<VkPushConstantRange>& pushConstantRanges, bool enableDepthTesting = false, VkFormat imageFormat = VK_FORMAT_B8G8R8A8_UNORM, VkColorComponentFlags colorComponentFlags = VKCOMP_RGBA_BITS, VkPipelineColorBlendAttachmentState colorBlendState = GetBlendDescription(true), VkPrimitiveTopology vertexTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VkPolygonMode polgyonTopology = VK_POLYGON_MODE_FILL)
			: vkdevice(vkdevice), imageFormat(imageFormat), vertexDescription(vertexDescription), shaders(shaders), descriptorBindings(descriptorBindings), pushConstantRanges(pushConstantRanges), colorComponentFlags(colorComponentFlags), colorBlendState(colorBlendState), vertexTopology(vertexTopology), polgyonTopology(polgyonTopology) {
				onDispose.hook(TinyCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));

				this->enableBlending = colorBlendState.blendEnable;
				this->enableDepthTesting = enableDepthTesting;

				TinyQueueFamily indices = vkdevice.QueryPhysicalDeviceQueueFamilies();
				vkGetDeviceQueue(vkdevice.logicalDevice, indices.graphicsFamily, 0, &graphicsQueue);
				
				if (vkdevice.presentSurface != VK_NULL_HANDLE)
					vkGetDeviceQueue(vkdevice.logicalDevice, indices.presentFamily, 0, &presentQueue);
			}

			/// @brief Returns the optimal VkFormat for the desired depth image format.
			VkFormat QueryDepthFormat(VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL) {
				const std::vector<VkFormat>& candidates = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
				for (VkFormat format : candidates) {
					VkFormatProperties props;
					vkGetPhysicalDeviceFormatProperties(vkdevice.physicalDevice, format, &props);

					VkFormatFeatureFlags features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
					if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
						return format;
					} else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
						return format;
					}
				}

				return VK_FORMAT_D32_SFLOAT;
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
			
			/// @brief Initialize the Graphics Pipeline and Read/Compile shaders.
			VkResult Initialize() {
				TinyQueueFamily indices = vkdevice.QueryPhysicalDeviceQueueFamilies();
				if (!indices.hasGraphicsFamily)
					return VK_ERROR_INITIALIZATION_FAILED;
				
				///////////////////////////////////////////////////////////////////////////////////////////////////////
				/////////// This section specifies that TinyVkVertex provides the vertex layout description ///////////
				const VkVertexInputBindingDescription bindingDescription = vertexDescription.binding;
				const std::vector<VkVertexInputAttributeDescription>& attributeDescriptions = vertexDescription.attributes;
				VkResult result = VK_SUCCESS;

				VkPipelineVertexInputStateCreateInfo vertexInputInfo {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
					.pVertexBindingDescriptions = &bindingDescription, .vertexBindingDescriptionCount = 1,
					.pVertexAttributeDescriptions = attributeDescriptions.data(), .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size())
				};
				
				///////////////////////////////////////////////////////////////////////////////////////////////////////
				///////////////////////////////////////////////////////////////////////////////////////////////////////
				
				VkPipelineLayoutCreateInfo pipelineLayoutInfo {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
					.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size()),
					.pPushConstantRanges = pushConstantRanges.data()
				};

				if (descriptorBindings.size() > 0) {
					VkDescriptorSetLayoutCreateInfo descriptorCreateInfo {
						.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
						.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
						.pBindings = descriptorBindings.data(), .bindingCount = static_cast<uint32_t>(descriptorBindings.size())
					};

					result = vkCreateDescriptorSetLayout(vkdevice.logicalDevice, &descriptorCreateInfo, VK_NULL_HANDLE, &descriptorLayout);
					if (result != VK_SUCCESS) return result;

					pipelineLayoutInfo.setLayoutCount = 1;
					pipelineLayoutInfo.pSetLayouts = &descriptorLayout;
				}

				result = vkCreatePipelineLayout(vkdevice.logicalDevice, &pipelineLayoutInfo, VK_NULL_HANDLE, &pipelineLayout);
				if (result != VK_SUCCESS) return result;
				
				///////////////////////////////////////////////////////////////////////////////////////////////////////
				///////////////////////////////////////////////////////////////////////////////////////////////////////
				
				VkPipelineInputAssemblyStateCreateInfo inputAssembly {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
					.topology = vertexTopology, .primitiveRestartEnable = VK_FALSE
				};

				VkPipelineViewportStateCreateInfo viewportState {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
					.viewportCount = 1, .scissorCount = 1, .flags = 0
				};

				VkPipelineRasterizationStateCreateInfo rasterizer {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
					.depthBiasEnable = VK_FALSE, .depthClampEnable = VK_FALSE, .rasterizerDiscardEnable = VK_FALSE,
					.polygonMode = polgyonTopology, .lineWidth = 1.0f,
					.cullMode = VK_CULL_MODE_BACK_BIT, .frontFace = VK_FRONT_FACE_CLOCKWISE
				};

				VkPipelineMultisampleStateCreateInfo multisampling {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
					.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT, .sampleShadingEnable = VK_FALSE,
				};

				VkPipelineColorBlendStateCreateInfo colorBlending {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
					.logicOpEnable = VK_FALSE, .logicOp = VK_LOGIC_OP_COPY,
					.attachmentCount = 1, .pAttachments = &colorBlendState,
					.blendConstants[0] = 0.0f, .blendConstants[1] = 0.0f, .blendConstants[2] = 0.0f, .blendConstants[3] = 0.0f
				};

				const std::array<VkDynamicState, 2> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
				VkPipelineDynamicStateCreateInfo dynamicState {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
					.pDynamicStates = dynamicStateEnables.data(), .dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size()),
					.flags = 0, .pNext = VK_NULL_HANDLE
				};

				VkPipelineRenderingCreateInfoKHR renderingCreateInfo {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
					.pColorAttachmentFormats = &imageFormat, .depthAttachmentFormat = QueryDepthFormat(), .colorAttachmentCount = 1
				};

				VkPipelineDepthStencilStateCreateInfo depthStencilInfo {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
					.depthTestEnable = enableDepthTesting, .depthWriteEnable = enableDepthTesting,
					.depthCompareOp = VK_COMPARE_OP_LESS, .depthBoundsTestEnable = VK_FALSE, .minDepthBounds = 0.0f, .maxDepthBounds = 1.0f,
					.stencilTestEnable = VK_FALSE, .front = {}, .back = {},
				};

				///////////////////////////////////////////////////////////////////////////////////////////////////////
				///////////////////////////////////////////////////////////////////////////////////////////////////////

				std::vector<VkPipelineShaderStageCreateInfo> shaderPipelineCreateInfo;
				std::vector<VkShaderModule> shaderModules;
				for (size_t i = 0; i < shaders.size(); i++) {
					auto shaderCode = ReadShaderFile(std::get<1>(shaders[i]));
					auto shaderModule = CreateShaderModule(shaderCode);

					if (shaderModule == VK_NULL_HANDLE) {
						result = VK_ERROR_INVALID_SHADER_NV;
						break;
					}

					#if TINY_ENGINE_VALIDATION
					std::cout << "TinyVulkan: Loading Shader @ " << std::get<1>(shaders[i]) << std::endl;
					#endif
					shaderModules.push_back(shaderModule);
					VkPipelineShaderStageCreateInfo shaderCreateInfo = {
						.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = (VkShaderStageFlagBits)std::get<0>(shaders[i]), .module = shaderModule, .pName = "main"
					};
					shaderPipelineCreateInfo.push_back(shaderCreateInfo);
				}
				
				VkGraphicsPipelineCreateInfo pipelineInfo {
					.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
					.stageCount = static_cast<uint32_t>(shaderPipelineCreateInfo.size()),
					.pStages = shaderPipelineCreateInfo.data(),
					.layout = pipelineLayout,
					.pVertexInputState = &vertexInputInfo,
					.pInputAssemblyState = &inputAssembly,
					.pViewportState = &viewportState,
					.pRasterizationState = &rasterizer,
					.pMultisampleState = &multisampling,
					.pColorBlendState = &colorBlending,
					.pDepthStencilState = &depthStencilInfo,
					.pDynamicState = &dynamicState,
					.pNext = &renderingCreateInfo,
					.renderPass = VK_NULL_HANDLE, .subpass = 0,
					.basePipelineIndex = -1, .basePipelineHandle = VK_NULL_HANDLE,
				};

				if (result == VK_SUCCESS)
					result = vkCreateGraphicsPipelines(vkdevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, VK_NULL_HANDLE, &graphicsPipeline);
				
				for(auto shaderModule : shaderModules)
					if (shaderModule != VK_NULL_HANDLE)
						vkDestroyShaderModule(vkdevice.logicalDevice, shaderModule, VK_NULL_HANDLE);
				
				return result;
			}

			/// @brief Constructor(...) + Initialize() with error result as combined TinyObject<Object,VkResult>.
			template<typename... A>
			inline static TinyObject<TinyGraphicsPipeline> Construct(TinyVkDevice& vkdevice, TinyVertexDescription vertexDescription, const std::vector<std::tuple<TinyShaderStages, std::string>> shaders, const std::vector<VkDescriptorSetLayoutBinding>& descriptorBindings, const std::vector<VkPushConstantRange>& pushConstantRanges, bool enableDepthTesting = false, VkFormat imageFormat = VK_FORMAT_B8G8R8A8_UNORM, VkColorComponentFlags colorComponentFlags = VKCOMP_RGBA_BITS, VkPipelineColorBlendAttachmentState colorBlendState = GetBlendDescription(true), VkPrimitiveTopology vertexTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VkPolygonMode polgyonTopology = VK_POLYGON_MODE_FILL) {
				std::unique_ptr<TinyGraphicsPipeline> object =
					std::make_unique<TinyGraphicsPipeline>(vkdevice, vertexDescription, shaders, descriptorBindings, pushConstantRanges, enableDepthTesting, imageFormat , colorComponentFlags, colorBlendState, vertexTopology, polgyonTopology);
				return TinyObject<TinyGraphicsPipeline>(object, object->Initialize());
			}	

			/// @brief Gets the number of bound descriptors for a particular shader stage.
			inline static uint32_t SelectBindingCountByShaderStage(TinyGraphicsPipeline& pipeline, TinyShaderStages flags) {
				return std::count_if(pipeline.descriptorBindings.begin(), pipeline.descriptorBindings.end(), [flags](VkDescriptorSetLayoutBinding binding) {
					return binding.stageFlags == (VkShaderStageFlags) flags;
				});
			}

			/// @brief Gets a generic Normal Blending Mode for creating a GraphicsPipeline with.
			inline static VkPipelineColorBlendAttachmentState GetBlendDescription(bool isBlendingEnabled = true) {
				return {
					.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
					.colorBlendOp = VK_BLEND_OP_ADD, .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA, .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
					.alphaBlendOp = VK_BLEND_OP_ADD, .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE, .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
					.blendEnable = isBlendingEnabled
				};
			}

			/// @brief Returns the push constant range info of a given size applied to the given shader stages.
			inline static VkPushConstantRange SelectPushConstantRange(uint32_t pushConstantRangeSize, TinyShaderStages shaderStages) {
				return { .stageFlags = (VkShaderStageFlags) shaderStages, .offset = 0, .size = pushConstantRangeSize };
			}

			/// @brief Creates a write image descriptor (Combined Image Sampler) for passing images to the GPU (on myrenderer.PushDescriptorSet).
			inline static VkWriteDescriptorSet SelectWriteImageDescriptor(uint32_t binding, uint32_t descriptorCount, const VkDescriptorImageInfo* imageInfo) {
				return { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pImageInfo = imageInfo, .dstSet = 0, .dstBinding = binding, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = descriptorCount };
			}

			/// @brief Creates a write buffer descriptor (any of VK_DESCRIPTOR_TYPE_*_BUFFER) for passing buffers to the GPU (on myrenderer.PushDescriptorSet).
			inline static VkWriteDescriptorSet SelectWriteBufferDescriptor(uint32_t binding, uint32_t descriptorCount, const VkDescriptorBufferInfo* bufferInfo) {
				return { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pBufferInfo = bufferInfo, .dstSet = 0, .dstBinding = binding, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = descriptorCount };
			}
		
			/// @brief Creates a layout description for how a descriptor should be bound to the graphics pipeline at hwta binding and shader stages./summary>
			inline static VkDescriptorSetLayoutBinding SelectPushDescriptorLayoutBinding(uint32_t binding, TinyDescriptorType descriptorType, TinyShaderStages stageFlags, uint32_t descriptorCount = 1) {
				return { .binding = binding, .descriptorType = static_cast<VkDescriptorType>(descriptorType), .descriptorCount = descriptorCount, .pImmutableSamplers = VK_NULL_HANDLE, .stageFlags = (VkShaderStageFlags) stageFlags };
			}
		};
	}
#endif

/*
	PIPELINE PUSH DESCRIPTORS:
		* Push Descriptors are bound to specific IDs for the graphics pipeline.
			The bound IDs are shared across all shaders in the pipeline.
			If the vertex shader binds a mat4 transform to binding ID = 0
			Then the fragment shader must bind its texture to binding ID = 1
*/