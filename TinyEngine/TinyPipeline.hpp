#ifndef TINY_ENGINE_RENDERPIPELINE
#define TINY_ENGINE_RENDERPIPELINE
	#include "./TinyEngine.hpp"

	namespace TINY_ENGINE_NAMESPACE {
		#define VKCOMP_RGBA_BITS VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		#define VKCOMP_BGRA_BITS VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_A_BIT
		
		struct TinyVertexDescription {
		public:
			const VkVertexInputBindingDescription binding;
			const std::vector<VkVertexInputAttributeDescription> attributes;

			TinyVertexDescription(VkVertexInputBindingDescription binding = {}, const std::vector<VkVertexInputAttributeDescription> attributes = {})
			: binding(binding), attributes(attributes) {}
		};

		struct TinyVertex {
		public:
			glm::vec2 texcoord;
            glm::vec3 position;
            glm::vec4 color;

            TinyVertex() : texcoord(glm::vec2(0.0)), position(glm::vec3(0.0)), color(glm::vec4(1.0)) {};
			TinyVertex(glm::vec2 tex, glm::vec3 pos, glm::vec4 col) : texcoord(tex), position(pos), color(col) {}

            static TinyVertexDescription GetVertexDescription() {
                return TinyVertexDescription(GetBindingDescription(), GetAttributeDescriptions());
            }

            static VkVertexInputBindingDescription GetBindingDescription() {
                return { .binding = 0, .stride = sizeof(TinyVertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
            }

            static const std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions() {
                return {
					{ .binding = 0, .location = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(TinyVertex, texcoord) },
					{ .binding = 0, .location = 1, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(TinyVertex, position) },
                	{ .binding = 0, .location = 2, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(TinyVertex, color) }
				};
            }
        };

		struct TinyShader {
			public:
				TinyShaderStages stage;
				std::string shaderpath;
				std::vector<uint32_t> pconstants;
				std::vector<std::pair<TinyDescriptorType, TinyDescriptorBinding>> pdescriptors;
				
				TinyShader(TinyShaderStages stage, std::string shaderpath, std::vector<uint32_t> pconstants = {}, std::vector<std::pair<TinyDescriptorType, TinyDescriptorBinding>> pdescriptors = {})
				: stage(stage), shaderpath(shaderpath), pconstants(pconstants), pdescriptors(pdescriptors) {}
		};

		struct TinyPipelineCreateInfo {
		public:
			std::vector<TinyShader> shaders;
			TinyPipelineType type;
			bool blending;
			bool interpolation;
			bool clearOnLoad;
			VkFormat imageFormat;
			VkSamplerAddressMode addressMode;
			VkPrimitiveTopology vertexTopology;
			VkPolygonMode polygonTopology;
			TinyVertexDescription vertexDescription;

			static TinyPipelineCreateInfo GraphicsInfo(TinyShader vertex, TinyShader fragment, bool blending = true, bool interpolation = false, bool clearOnLoad = true, VkFormat imageFormat = VK_FORMAT_B8G8R8A8_UNORM, VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VkPrimitiveTopology vertexTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VkPolygonMode polygonTopology = VK_POLYGON_MODE_FILL, TinyVertexDescription vertexDescription = TinyVertex::GetVertexDescription()) {
				return { {vertex, fragment}, TinyPipelineType::TYPE_GRAPHICS, blending, interpolation, clearOnLoad, imageFormat, addressMode, vertexTopology, polygonTopology, vertexDescription };
			}

			static TinyPipelineCreateInfo PresentInfo(TinyShader vertex, TinyShader fragment, bool blending = true, bool interpolation = false, bool clearOnLoad = true, VkFormat imageFormat = VK_FORMAT_B8G8R8A8_UNORM, VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VkPrimitiveTopology vertexTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VkPolygonMode polygonTopology = VK_POLYGON_MODE_FILL, TinyVertexDescription vertexDescription = TinyVertex::GetVertexDescription()) {
				return { {vertex, fragment}, TinyPipelineType::TYPE_PRESENT, blending, interpolation, clearOnLoad, imageFormat, addressMode, vertexTopology, polygonTopology, vertexDescription };
			}

			static TinyPipelineCreateInfo TransferInfo() {
				return { {}, TinyPipelineType::TYPE_TRANSFER, true, false, false, VK_FORMAT_B8G8R8A8_UNORM, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_FILL, TinyVertex::GetVertexDescription() };
			}
		};

		class TinyPipeline : public TinyDisposable {
		public:
			TinyVkDevice& vkdevice;
			TinyPipelineCreateInfo createInfo;
			VkPipelineLayout layout = VK_NULL_HANDLE;
			VkPipeline pipeline = VK_NULL_HANDLE;
			VkQueue submitQueue = VK_NULL_HANDLE;
			VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
			VkResult initialized = VK_ERROR_INITIALIZATION_FAILED;

			TinyPipeline operator=(const TinyPipeline&) = delete;
			TinyPipeline(const TinyPipeline&) = delete;
			~TinyPipeline() { this->Dispose(); }
			
			void Disposable(bool waitIdle) {
				if (waitIdle) { vkQueueWaitIdle(submitQueue); vkDeviceWaitIdle(vkdevice.logicalDevice); }
				if (descriptorLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(vkdevice.logicalDevice, descriptorLayout, VK_NULL_HANDLE);
				if (pipeline != VK_NULL_HANDLE) vkDestroyPipeline(vkdevice.logicalDevice, pipeline, VK_NULL_HANDLE);
				if (layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(vkdevice.logicalDevice, layout, VK_NULL_HANDLE);
			}

			TinyPipeline(TinyVkDevice& vkdevice, TinyPipelineCreateInfo createInfo)
			: vkdevice(vkdevice), createInfo(createInfo) {
				onDispose.hook(TinyCallback<bool>([this](bool forceDispose) {this->Disposable(forceDispose); }));
				initialized = Initialize();
			}
			
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

			VkShaderModule CreateShaderModule(std::vector<char> shaderCode) {
				VkShaderModuleCreateInfo createInfo {
					.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
					.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data()), .codeSize = shaderCode.size(), .flags = 0, .pNext = VK_NULL_HANDLE,
				};
				VkShaderModule shaderModule;
				vkCreateShaderModule(vkdevice.logicalDevice, &createInfo, VK_NULL_HANDLE, &shaderModule);
				return shaderModule;
			}
			
			VkResult Initialize() {
				if (!vkdevice.queueFamilyIndices.hasGraphicsFamily && !vkdevice.queueFamilyIndices.hasPresentFamily) return VK_ERROR_INITIALIZATION_FAILED;
				
				switch(createInfo.type) {
					case TinyPipelineType::TYPE_GRAPHICS:
					case TinyPipelineType::TYPE_TRANSFER:
						vkGetDeviceQueue(vkdevice.logicalDevice, vkdevice.queueFamilyIndices.graphicsFamily, 0, &submitQueue);
					break;
					case TinyPipelineType::TYPE_PRESENT:
						vkGetDeviceQueue(vkdevice.logicalDevice, vkdevice.queueFamilyIndices.presentFamily, 0, &submitQueue);
					break;
				}

				VkResult result = VK_SUCCESS;
				if (createInfo.type == TinyPipelineType::TYPE_TRANSFER) {
					#if TINY_ENGINE_VALIDATION
						std::cout << "TinyEngine: Created transfer / staging pipeline." << std::endl;
					#endif
					return result;
				}

				std::vector<VkPipelineShaderStageCreateInfo> shaderPipelineCreateInfo;
				std::vector<VkShaderModule> shaderModules;
				for(TinyShader shader : createInfo.shaders) {
					std::vector<char> shaderCode = ReadShaderFile(shader.shaderpath);
					VkShaderModule shaderModule = CreateShaderModule(shaderCode);

					if (shaderModule != VK_NULL_HANDLE) {
						shaderModules.push_back(shaderModule);
						shaderPipelineCreateInfo.push_back({ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = (VkShaderStageFlagBits) shader.stage, .module = shaderModule, .pName = "main" });
					} else { result = VK_ERROR_INVALID_SHADER_NV; break; }
				}
				
				if (result == VK_SUCCESS) {
					std::vector<VkPushConstantRange> pconstants;
					for(TinyShader shader : createInfo.shaders)
						for(uint32_t range : shader.pconstants)
							pconstants.push_back(TinyPipeline::GetPushConstantRange(shader.stage, range));
	
					std::vector<VkDescriptorSetLayoutBinding> pdescriptors;
					for(TinyShader shader : createInfo.shaders)
						for(std::pair<TinyDescriptorType, TinyDescriptorBinding> type : shader.pdescriptors)
							pdescriptors.push_back(TinyPipeline::GetPushDescriptorLayoutBinding(shader.stage, static_cast<uint32_t>(type.second), type.first, 1));
					
					VkDescriptorSetLayoutCreateInfo descriptorCreateInfo { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR, .pBindings = pdescriptors.data(), .bindingCount = static_cast<uint32_t>(pdescriptors.size()) };
					result = vkCreateDescriptorSetLayout(vkdevice.logicalDevice, &descriptorCreateInfo, VK_NULL_HANDLE, &descriptorLayout);

					VkPipelineVertexInputStateCreateInfo vertexInputInfo = defaultVertexInputInfo;
						vertexInputInfo.vertexBindingDescriptionCount = 1;
						vertexInputInfo.pVertexBindingDescriptions = &createInfo.vertexDescription.binding;
						vertexInputInfo.pVertexAttributeDescriptions = createInfo.vertexDescription.attributes.data();
						vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(createInfo.vertexDescription.attributes.size());
					VkPipelineInputAssemblyStateCreateInfo inputAssembly = defaultInputAssembly;
						inputAssembly.topology = createInfo.vertexTopology;
					VkPipelineRasterizationStateCreateInfo rasterizer = defaultRasterizer;
						rasterizer.polygonMode = createInfo.polygonTopology;
					VkPipelineColorBlendAttachmentState colorBlendState = defaultColorBlendState;
						colorBlendState.blendEnable = createInfo.blending;
					VkPipelineColorBlendStateCreateInfo colorBlending = defaultColorBlending;
						colorBlending.pAttachments = &colorBlendState;
					VkPipelineRenderingCreateInfoKHR renderingCreateInfo = defaultRenderingCreateInfo;
						renderingCreateInfo.pColorAttachmentFormats = &createInfo.imageFormat;
					
					VkPipelineLayoutCreateInfo pipelineLayoutInfo {
						.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
						.pushConstantRangeCount = static_cast<uint32_t>(pconstants.size()),
						.pPushConstantRanges = pconstants.data(),
						.setLayoutCount = (descriptorLayout != VK_NULL_HANDLE)? 1U : 0U, .pSetLayouts = &descriptorLayout
					};
					result = vkCreatePipelineLayout(vkdevice.logicalDevice, &pipelineLayoutInfo, VK_NULL_HANDLE, &layout);
					
					VkGraphicsPipelineCreateInfo graphicsPipelineInfo {
						.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
						.stageCount = static_cast<uint32_t>(shaderPipelineCreateInfo.size()),
						.pStages = shaderPipelineCreateInfo.data(),
						.layout = layout,
						.pVertexInputState = &vertexInputInfo,
						.pInputAssemblyState = &inputAssembly,
						.pViewportState = &defaultViewportState,
						.pRasterizationState = &rasterizer,
						.pMultisampleState = &defaultMultisampling,
						.pColorBlendState = &colorBlending,
						.pDepthStencilState = &defaultDepthStencilInfo,
						.pDynamicState = &defaultDynamicState,
						.pNext = &renderingCreateInfo,
						.renderPass = VK_NULL_HANDLE, .subpass = 0, .basePipelineIndex = -1, .basePipelineHandle = VK_NULL_HANDLE
					};

					switch(createInfo.type) {
						case TinyPipelineType::TYPE_GRAPHICS:
							result = vkCreateGraphicsPipelines(vkdevice.logicalDevice, VK_NULL_HANDLE, 1, &graphicsPipelineInfo, VK_NULL_HANDLE, &pipeline);
							#if TINY_ENGINE_VALIDATION
								std::cout << "TinyEngine: Created graphics render pipeline." << std::endl;
							#endif
						break;
						case TinyPipelineType::TYPE_PRESENT:
							result = vkCreateGraphicsPipelines(vkdevice.logicalDevice, VK_NULL_HANDLE, 1, &graphicsPipelineInfo, VK_NULL_HANDLE, &pipeline);
							#if TINY_ENGINE_VALIDATION
								std::cout << "TinyEngine: Created present render pipeline." << std::endl;
							#endif
						break;
						default: break;
					}
				}

				for(auto shaderModule : shaderModules)
					if (shaderModule != VK_NULL_HANDLE)
						vkDestroyShaderModule(vkdevice.logicalDevice, shaderModule, VK_NULL_HANDLE);
				return result;
			}
			
			inline static VkPushConstantRange GetPushConstantRange(TinyShaderStages shaderStages, uint32_t pushConstantRangeSize) {
				return { .stageFlags = (VkShaderStageFlags) shaderStages, .offset = 0, .size = pushConstantRangeSize };
			}
			
			inline static VkDescriptorSetLayoutBinding GetPushDescriptorLayoutBinding(TinyShaderStages stageFlags, uint32_t binding, TinyDescriptorType descriptorType, uint32_t descriptorCount = 1) {
				return { .binding = binding, .descriptorType = static_cast<VkDescriptorType>(descriptorType), .descriptorCount = descriptorCount, .pImmutableSamplers = VK_NULL_HANDLE, .stageFlags = (VkShaderStageFlags) stageFlags };
			}
		};
	}
#endif