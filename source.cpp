#include "./TinyEngine/TinyEngine.hpp"
using namespace tny;

#define DEFAULT_FRAGMENT_SHADER "./Shaders/default_output_frag.spv"
#define SPRITE_VERTEX_SHADER "./Shaders/texture_output_vert.spv"
#define SPRITE_FRAGMENT_SHADER "./Shaders/texture_output_frag.spv"

int TINY_ENGINE_WINDOWMAIN {
    TinyWindow window("Tiny Engine", 1920, 1080, true, false, true, false, true, 640, 480);
    TinyVkDevice vkdevice(&window);
    TinyCommandPool cmdpool1(vkdevice);
    TinyCommandPool cmdpool2(vkdevice);
    TinyCommandPool cmdpool3(vkdevice);

    TinyShader vertexShader(TinyShaderStages::STAGE_VERTEX, SPRITE_VERTEX_SHADER, { sizeof(glm::mat4) }, {});
    TinyShader defaultFragShader(TinyShaderStages::STAGE_FRAGMENT, DEFAULT_FRAGMENT_SHADER, {}, {});
    TinyShader fragShader(TinyShaderStages::STAGE_FRAGMENT, SPRITE_FRAGMENT_SHADER, {}, {{TinyDescriptorType::TYPE_IMAGE_SAMPLER, TinyDescriptorBinding::BINDING_0}});
    
    TinyPipeline pipeline1(vkdevice, TinyPipelineCreateInfo::TransferInfo());
    TinyPipeline pipeline2(vkdevice, TinyPipelineCreateInfo::GraphicsInfo(vertexShader, defaultFragShader, true, false, VK_FORMAT_B8G8R8A8_UNORM));
    TinyPipeline pipeline3(vkdevice, TinyPipelineCreateInfo::PresentInfo(vertexShader, fragShader, true, false, VK_FORMAT_B8G8R8A8_UNORM));
    TinyRenderGraph graph(vkdevice, &window);
    
    std::vector<TinyRenderPass*> renderpass1 = graph.CreateRenderPass(cmdpool1, pipeline1, "Staging Data Pass", {VkExtent2D(1920, 1080)}, 1);
    std::vector<TinyRenderPass*> renderpass2 = graph.CreateRenderPass(cmdpool2, pipeline2, "Texture Input Pass", {VkExtent2D(1920, 1080)}, 1);
    graph.ResizeImageWithSwapchain(renderpass2[0]->targetImage);
    std::vector<TinyRenderPass*> renderpass3 = graph.CreateRenderPass(cmdpool3, pipeline3, "Copy Pass", {{ 1920, 1080 }}, 1);
    renderpass2[0]->AddDependency(*renderpass1[0]);
    renderpass3[0]->AddDependency(*renderpass2[0]);
    
    std::vector<TinyVertex> triangles = TinyQuad::Create(glm::vec4(0.0, 0.0, 500, 500), 1.0);
    TinyQuad::Reposition(triangles, glm::vec2(240.0f, 135.0f), false);
    std::vector<TinyVertex> triangles2 = TinyQuad::Create(glm::vec4(0.0, 0.0, window.hwndWidth, window.hwndHeight), 1.0);
    triangles.insert(triangles.end(), triangles2.begin(), triangles2.end());

    size_t sizeofTriangles = TinyMath::GetSizeofVector<TinyVertex>(triangles);
    TinyBuffer vbuffer(vkdevice, TinyBufferType::TYPE_VERTEX, sizeofTriangles);
    TinyBuffer stagingBuffer(vkdevice, TinyBufferType::TYPE_STAGING, sizeofTriangles);
    
    renderpass1[0]->renderEvent.hook(TinyRenderEvent([&](TinyRenderPass& renderPass, TinyRenderObject& renderer, bool frameResized) {
        VkDeviceSize offset = 0;

        triangles = TinyQuad::Create(glm::vec4(0.0, 0.0, 500, 500), 1.0);
        TinyQuad::Reposition(triangles, glm::vec2(240.0f, 135.0f), false);
        triangles2 = TinyQuad::Create(glm::vec4(0.0, 0.0, window.hwndWidth, window.hwndHeight), 1.0);
        triangles.insert(triangles.end(), triangles2.begin(), triangles2.end());

        renderer.StageBuffer(stagingBuffer, vbuffer, triangles.data(), sizeofTriangles, offset);
    }));

    glm::mat4 camera = TinyMath::Project2D(window.hwndWidth, window.hwndHeight, 0.0, 0.0, 1.0, 0.0);
    //TinyObject<TinyBuffer> projection = TinyBuffer::Construct(vkdevice, TinyBufferType::TYPE_UNIFORM, sizeof(glm::mat4));

    renderpass2[0]->renderEvent.hook(TinyRenderEvent([&](TinyRenderPass& renderPass, TinyRenderObject& renderer, bool frameResized) {
        camera = TinyMath::Project2D(window.hwndWidth, window.hwndHeight, 0.0, 0.0, 1.0, 0.0);
        renderer.PushConstant(TinyShaderStages::STAGE_VERTEX, &camera, sizeof(glm::mat4));
        renderer.BindVertices(vbuffer, 0);
        renderer.DrawInstances(6, 1, 0, 0);
        renderer.SetClearColor(0, 0, 0, 255);
        //renderer.SetRenderArea(0, 0, window.hwndWidth, window.hwndHeight);
    }));

    renderpass3[0]->renderEvent.hook(TinyRenderEvent([&](TinyRenderPass& renderPass, TinyRenderObject& renderer, bool frameResized) {
        camera = TinyMath::Project2D(window.hwndWidth, window.hwndHeight, 0.0, 0.0, 1.0, 0.0);
        renderer.PushImage(*renderPass.dependencies[0]->targetImage, 0);
        renderer.PushConstant(TinyShaderStages::STAGE_VERTEX, &camera, sizeof(glm::mat4));
        renderer.BindVertices(vbuffer, 0);
        renderer.DrawInstances(6, 1, 6, 0);
        renderer.SetClearColor(255, 0, 0, 255);
        //renderer.SetRenderArea(0, 0, window.hwndWidth, window.hwndHeight);
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

    /*renderpass1[0]->onRender.hook(TinyRenderEvent([&](TinyRenderPass& renderpass, std::vector<VkCommandBuffer>& writeCmdBuffers, bool frameResized) {
        triangles2 = TinyQuad::Create(glm::vec4(0.0, 0.0, window.hwndWidth, window.hwndHeight), 1.0, TinyQuad::defvcolors);
        
        auto cmdbuffer = renderpass.BeginStageCmdBuffer();
            VkDeviceSize offset = 0;
            renderpass.StageBuffer(cmdbuffer, stagingBuffer, ibuffer, indices.data(), sizeofIndices, offset);
            renderpass.StageBuffer(cmdbuffer, stagingBuffer, vbuffer, triangles.data(), sizeofTriangles, offset);
            renderpass.StageBuffer(cmdbuffer, stagingBuffer, vbuffer2, triangles2.data(), sizeofTriangles2, offset);
        renderpass.EndStageCmdBuffer(cmdbuffer);
        writeCmdBuffers.push_back(cmdbuffer.first);
    }));*/
    
    /*renderpass2[0]->onRender.hook(TinyRenderEvent([&](TinyRenderPass& renderpass, std::vector<VkCommandBuffer>& writeCmdBuffers, bool frameResized) {
        auto cmdbuffer = renderpass.BeginRecordCmdBuffer();
            if (frameResized)
                camera = TinyMath::Project2D(window.hwndWidth, window.hwndHeight, 0, 0, 1.0, 0.0);
            
            renderpass.PushConstants(cmdbuffer, TinyShaderStages::STAGE_VERTEX, sizeof(glm::mat4), &camera);
            renderpass.CmdBindGeometryVI(cmdbuffer, &vbuffer.buffer, ibuffer.buffer);
            renderpass.CmdDrawGeometry(cmdbuffer, true, 1, indices.size());
        renderpass.EndRecordCmdBuffer(cmdbuffer);
        writeCmdBuffers.push_back(cmdbuffer.first);
    }));*/
    
    /*renderpass3[0]->onRender.hook(TinyRenderEvent([&](TinyRenderPass& renderpass, std::vector<VkCommandBuffer>& writeCmdBuffers, bool frameResized) {
        if (frameResized)
            camera = TinyMath::Project2D(window.hwndWidth, window.hwndHeight, 0, 0, 1.0, 0.0);
        
        auto cmdbuffer = renderpass.BeginRecordCmdBuffer();
            VkDescriptorImageInfo imageDescriptor = renderpass.dependencies[0]->targetImage->GetDescriptorInfo();
            VkWriteDescriptorSet imageDescriptorSet = renderpass.dependencies[0]->targetImage->GetWriteDescriptor(0, 1, &imageDescriptor);
            renderpass.PushDescriptorSet(cmdbuffer, { imageDescriptorSet });
            renderpass.PushConstants(cmdbuffer, TinyShaderStages::STAGE_VERTEX, sizeof(glm::mat4), &camera);
            renderpass.CmdBindGeometryVI(cmdbuffer, &vbuffer2.buffer, ibuffer.buffer);
            renderpass.CmdDrawGeometry(cmdbuffer, true, 1, indices.size());
        renderpass.EndRecordCmdBuffer(cmdbuffer);
        writeCmdBuffers.push_back(cmdbuffer.first);
    }));*/