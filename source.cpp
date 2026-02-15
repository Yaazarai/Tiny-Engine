#include "./TinyEngine/TinyEngine.hpp"
using namespace tny;
using namespace glm;

#define QOI_IMPLEMENTATION
#include "./TinyEngine/Externals/qoi.h"
#define DEFAULT_FRAGMENT_SHADER "./Shaders/default_output_frag.spv"
#define SPRITE_VERTEX_SHADER "./Shaders/texture_output_vert.spv"
#define SPRITE_FRAGMENT_SHADER "./Shaders/texture_output_frag.spv"
#define DEFAULT_QOI_IMAGE "./Images/icons_default.qoi"

int TINY_ENGINE_WINDOWMAIN {
    TinyWindow window("Tiny Engine", 1920, 1080, true, false, true, false, true, 640, 480);
    TinyVkDevice vkdevice(&window);
    TinyCommandPool cmdpool(vkdevice);
    TinyRenderGraph graph(vkdevice, &window);

    TinyShader vertexShader(TinyShaderStages::STAGE_VERTEX, SPRITE_VERTEX_SHADER, { sizeof(mat4) });
    TinyShader defaultFragShader(TinyShaderStages::STAGE_FRAGMENT, DEFAULT_FRAGMENT_SHADER);
    TinyShader fragShader(TinyShaderStages::STAGE_FRAGMENT, SPRITE_FRAGMENT_SHADER, {}, {{TinyDescriptorType::TYPE_IMAGE_SAMPLER, TinyDescriptorBinding::BINDING_0}});
    
    TinyPipeline pipeline1(vkdevice, TinyPipelineCreateInfo::TransferInfo());
    TinyPipeline pipeline2(vkdevice, TinyPipelineCreateInfo::GraphicsInfo(vertexShader, fragShader, true, false, true, VK_FORMAT_B8G8R8A8_UNORM));
    TinyPipeline pipeline3(vkdevice, TinyPipelineCreateInfo::PresentInfo(vertexShader, fragShader, true, false, true, VK_FORMAT_B8G8R8A8_UNORM));

    TinyImage targetImage(vkdevice, TinyImageType::TYPE_COLORATTACHMENT, window.hwndWidth, window.hwndHeight);
    
    std::vector<TinyRenderPass*> renderpass1 = graph.CreateRenderPass(cmdpool, pipeline1, VK_NULL_HANDLE, "Staging Data Pass", 1);
    std::vector<TinyRenderPass*> renderpass2 = graph.CreateRenderPass(cmdpool, pipeline2, &targetImage, "Render Pass", 1);
    std::vector<TinyRenderPass*> renderpass3 = graph.CreateRenderPass(cmdpool, pipeline3, VK_NULL_HANDLE, "Copy Pass", 1);
    renderpass2[0]->AddDependency(renderpass1[0]);
    renderpass3[0]->AddDependency(renderpass2[0]);
    
    qoi_desc sourceImageDesc;
    void* sourceImageData = qoi_read(DEFAULT_QOI_IMAGE, &sourceImageDesc, 4);
    TinyImage sourceImage(vkdevice, TinyImageType::TYPE_COLORATTACHMENT, sourceImageDesc.width, sourceImageDesc.height);
    
    TinyQuad imageQuad(vec2(500.0, 500.0), 1.0, vec2(0.0, 0.0), vec2(0.0, 0.0), vec2(0.0, 0.0), 0.0, vec4(0.0, 0.0, 1.0, 1.0));
    TinyQuad screenQuad(vec2(window.hwndWidth, window.hwndHeight), 1.0, vec2(0.0, 0.0), vec2(0.0, 0.0), vec2(0.0, 0.0), 0.0, vec4(0.0, 0.0, 1.0, 1.0));

    size_t sizeofQuads = imageQuad.SizeofQuad() + screenQuad.SizeofQuad();
    size_t sizeOfImage = sourceImageDesc.width * sourceImageDesc.height * sourceImageDesc.channels;
    TinyBuffer vertexBuffer(vkdevice, TinyBufferType::TYPE_VERTEX, sizeofQuads);
    TinyBuffer stagingBuffer(vkdevice, TinyBufferType::TYPE_STAGING, sizeofQuads + sizeOfImage);

    renderpass1[0]->renderEvent.hook(TinyRenderEvent([&](TinyRenderPass& renderPass, TinyRenderObject& renderer, bool frameResized) {
        screenQuad.Resize(vec2(window.hwndWidth, window.hwndHeight));
        std::vector<TinyVertex> quads = TinyQuad::GetVertexVector({ imageQuad.Vertices(), screenQuad.Vertices() });

        VkDeviceSize offset = 0;
        renderer.StageBufferToBuffer(stagingBuffer, vertexBuffer, quads.data(), sizeofQuads, offset);
        
        VkDeviceSize byteSize = sourceImageDesc.width * sourceImageDesc.height * sourceImageDesc.channels;
        renderer.StageBufferToImage(stagingBuffer, sourceImage, sourceImageData, { .extent = { sourceImageDesc.width, sourceImageDesc.height}, .offset = {0, 0} }, byteSize, offset);
    }));

    mat4 camera = TinyMath::Project2D(window.hwndWidth, window.hwndHeight, 0.0, 0.0, 1.0, 0.0);

    renderpass2[0]->renderEvent.hook(TinyRenderEvent([&](TinyRenderPass& renderPass, TinyRenderObject& renderer, bool frameResized) {
        camera = TinyMath::Project2D(window.hwndWidth, window.hwndHeight, 0.0, 0.0, 1.0, 0.0);
        renderer.PushImage(sourceImage, 0);
        renderer.PushConstant(&camera, TinyShaderStages::STAGE_VERTEX, sizeof(mat4));
        renderer.BindVertices(vertexBuffer, 0);
        renderer.DrawInstances(6, 1, 0, 0);
    }));

    renderpass3[0]->renderEvent.hook(TinyRenderEvent([&](TinyRenderPass& renderPass, TinyRenderObject& renderer, bool frameResized) {
        camera = TinyMath::Project2D(window.hwndWidth, window.hwndHeight, 0.0, 0.0, 1.0, 0.0);
        renderer.PushImage(targetImage, 0);
        renderer.PushConstant(&camera, TinyShaderStages::STAGE_VERTEX, sizeof(mat4));
        renderer.BindVertices(vertexBuffer, 0);
        renderer.DrawInstances(6, 1, 6, 0);
    }));
    
    std::thread mythread([&window, &graph, &vkdevice]() {
        while (window.ShouldExecute()) {
            graph.RenderSwapChain();

            #if TINY_ENGINE_VALIDATION
                for(TinyRenderPass* pass : graph.renderPasses) {
                    std::vector<float> timestamps = pass->QueryTimeStamps();
                    
                    for(float time : timestamps)
                        std::cout << " - [" << graph.frameCounter << "] " << pass->subpassIndex << " : " << pass->title << " - " << time << " ms" << std::endl;
                    
                    for(TinyRenderPass* dependency : pass->dependencies)
                        std::cout << "\t wait: " << dependency->title << " (" << dependency->subpassIndex << ")" << std::endl;
                
                }
            #endif
        }
    });
    window.WhileMain(TinyWindowEvents::WAIT_EVENTS);
    
    mythread.join();
    vkDeviceWaitIdle(vkdevice.logicalDevice);
    return VK_SUCCESS;
};