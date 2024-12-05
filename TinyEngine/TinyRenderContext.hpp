#pragma once
#ifndef TINY_ENGINE_TINYRENDERCONTEXT
#define TINY_ENGINE_TINYRENDERCONTEXT

    #include "./TinyEngine.hpp"

    namespace TINY_ENGINE_NAMESPACE {
        /// @brief Graphics pipeline render context for the Image and Swapchain renderers.
        class TinyRenderContext : public TinyRenderInterface {
        public:
            TinyVkDevice& vkdevice;
            TinyCommandPool& commandPool;
            TinyGraphicsPipeline& graphicsPipeline;

            /// @brief Remove default copy destructor.
            TinyRenderContext(const TinyRenderContext&) = delete;
            
            /// @brief Remove default copy destructor.
			TinyRenderContext& operator=(const TinyRenderContext&) = delete;

            /// @brief Creates a reference context for the render pipeline for TinyRenderer/TinySwapchain.
            TinyRenderContext(TinyVkDevice& vkdevice, TinyCommandPool& commandPool, TinyGraphicsPipeline& graphicsPipeline)
            : vkdevice(vkdevice), commandPool(commandPool), graphicsPipeline(graphicsPipeline) {}

			/// @brief Default initialize() does nothing for TinyRenderContext.
            VkResult Initialize() { return VK_SUCCESS; }
            
            /// @brief Constructor(...) + Initialize() with error result as combined TinyObject<Object,VkResult>.
			template<typename... A>
			inline static TinyObject<TinyRenderContext> Construct(TinyVkDevice& vkdevice, TinyCommandPool& commandPool, TinyGraphicsPipeline& graphicsPipeline) {
				std::unique_ptr<TinyRenderContext> object =
					std::make_unique<TinyRenderContext>(vkdevice, commandPool, graphicsPipeline);
				return TinyObject<TinyRenderContext>(object, object->Initialize());
			}
        };
    }

#endif