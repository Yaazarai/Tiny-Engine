#include "./TinyEngine/TinyEngine.hpp"
using namespace tny;

//efine DEFAULT_TEXTURE "./Images/icons_default.qoi"
#define DEFAULT_FRAGMENT_SHADER "./Shaders/default_output_frag.spv"
#define SPRITE_VERTEX_SHADER "./Shaders/texture_output_vert.spv"
#define SPRITE_FRAGMENT_SHADER "./Shaders/texture_output_frag.spv"

int TINY_ENGINE_WINDOWMAIN {
    TinyWindow window ("Tiny Engine", 1920, 1080, true, false, true, false, true, 640, 480);
    TinyVkDevice vkdevice(true, true, false, false, &window);
    TinyCommandPool cmdpool(vkdevice, false);

    TinyShader vertexShader(TinyShaderStages::STAGE_VERTEX, SPRITE_VERTEX_SHADER,{ sizeof(glm::mat4) }, {});
    TinyShader defaultFragShader(TinyShaderStages::STAGE_FRAGMENT, DEFAULT_FRAGMENT_SHADER, {}, {{TinyDescriptorType::TYPE_IMAGE_SAMPLER, TinyDescriptorBinding::BINDING_0}});
    TinyShader fragShader(TinyShaderStages::STAGE_FRAGMENT, SPRITE_FRAGMENT_SHADER, {}, {{TinyDescriptorType::TYPE_IMAGE_SAMPLER, TinyDescriptorBinding::BINDING_0}});
    
    TinyPipeline pipeline1(vkdevice, TinyPipelineCreateInfo::TransferInfo());
    TinyPipeline pipeline2(vkdevice, TinyPipelineCreateInfo::GraphicsInfo(vertexShader, defaultFragShader, true, false, VK_FORMAT_B8G8R8A8_UNORM));
    TinyPipeline pipeline3(vkdevice, TinyPipelineCreateInfo::PresentInfo(vertexShader, fragShader, true, false, VK_FORMAT_B8G8R8A8_UNORM));
    TinyRenderGraph graph(vkdevice, &window);
    
    std::vector<TinyRenderPass*> renderpass1 = graph.CreateRenderPass(cmdpool, pipeline1, "Staging Data Pass", { 1920, 1080 }, 1, 0);
    std::vector<TinyRenderPass*> renderpass2 = graph.CreateRenderPass(cmdpool, pipeline2, "Texture Input Pass", { 1920, 1080 }, 2, 0);
    std::vector<TinyRenderPass*> renderpass3 = graph.CreateRenderPass(cmdpool, pipeline3, "Copy Pass", { 1920, 1080 }, 1, 0);
    renderpass2[0]->AddDependency(*renderpass1[0]);
    renderpass2[1]->AddDependency(*renderpass2[0]);
    renderpass3[0]->AddDependency(*renderpass2[1]);
    
    std::vector<TinyVertex> triangles = {
        TinyVertex({0.0f,0.0f}, {240.0f,135.0f,               0.0f}, {1.0f,0.0f,0.0f,1.0f}),
        TinyVertex({0.0f,0.0f}, {240.0f+960.0f,135.0f,        0.0f}, {0.0f,1.0f,0.0f,1.0f}),
        TinyVertex({0.0f,0.0f}, {240.0f+960.0f,135.0f+540.0f, 0.0f}, {1.0f,0.0f,1.0f,1.0f}),
        TinyVertex({0.0f,0.0f}, {240.0f,135.0f + 540.0f,      0.0f}, {0.0f,0.0f,1.0f,1.0f})
    };
    std::vector<TinyVertex> triangles2 = TinyQuad::Create(glm::vec4(0.0, 0.0, 1920.0, 1080.0), 1.0, TinyQuad::defvcolors);
    std::vector<uint32_t> indices = {0,1,2,2,3,0};

    size_t sizeofTriangles = TinyMath::GetSizeofVector<TinyVertex>(triangles);
    TinyBuffer vbuffer(vkdevice, TinyBufferType::TYPE_VERTEX, sizeofTriangles);
    size_t sizeofIndices = TinyMath::GetSizeofVector<uint32_t>(indices);
    TinyBuffer ibuffer(vkdevice, TinyBufferType::TYPE_INDEX, sizeofIndices);
    size_t sizeofTriangles2 = TinyMath::GetSizeofVector<TinyVertex>(triangles2);
    TinyBuffer vbuffer2(vkdevice, TinyBufferType::TYPE_VERTEX, sizeofTriangles2);
    
    glm::mat4 camera = TinyMath::Project2D(window.hwndWidth, window.hwndHeight, 0, 0, 1.0, 0.0);
    //TinyObject<TinyBuffer> projection = TinyBuffer::Construct(vkdevice, TinyBufferType::TYPE_UNIFORM, sizeof(glm::mat4));

    VkDeviceSize stagingSize = glm::pow(2, glm::ceil(glm::log2(static_cast<float>(sizeofTriangles + sizeofTriangles2 + sizeofIndices + sizeof(glm::mat4)))));
    TinyBuffer stagingBuffer(vkdevice, TinyBufferType::TYPE_STAGING, stagingSize);

    renderpass1[0]->onRender.hook(TinyCallback<TinyRenderPass&, TinyCommandPool&, std::vector<VkCommandBuffer>&, bool>([&](TinyRenderPass& renderpass, TinyCommandPool& commandPool, std::vector<VkCommandBuffer>& writeCmdBuffers, bool frameResized) {
        triangles2 = TinyQuad::Create(glm::vec4(0.0, 0.0, window.hwndWidth, window.hwndHeight), 1.0, TinyQuad::defvcolors);
        
        auto cmdbuffer = renderpass.BeginStageCmdBuffer();
            VkDeviceSize offset = 0;
            renderpass.StageBuffer(cmdbuffer, stagingBuffer, ibuffer, indices.data(), sizeofIndices, offset);
            renderpass.StageBuffer(cmdbuffer, stagingBuffer, vbuffer, triangles.data(), sizeofTriangles, offset);
            renderpass.StageBuffer(cmdbuffer, stagingBuffer, vbuffer2, triangles2.data(), sizeofTriangles2, offset);
        renderpass.EndStageCmdBuffer(cmdbuffer);
        writeCmdBuffers.push_back(cmdbuffer.first);
    }));
    
    auto texturePassCallback = TinyCallback<TinyRenderPass&, TinyCommandPool&, std::vector<VkCommandBuffer>&, bool>([&](TinyRenderPass& renderpass, TinyCommandPool& commandPool, std::vector<VkCommandBuffer>& writeCmdBuffers, bool frameResized) {
        auto cmdbuffer = renderpass.BeginRecordCmdBuffer();
            if (frameResized)
                camera = TinyMath::Project2D(window.hwndWidth, window.hwndHeight, 0, 0, 1.0, 0.0);
            
            renderpass.PushConstants(cmdbuffer, TinyShaderStages::STAGE_VERTEX, sizeof(glm::mat4), &camera);
            renderpass.CmdBindGeometry(cmdbuffer, &vbuffer.buffer, ibuffer.buffer);
            renderpass.CmdDrawGeometry(cmdbuffer, true, 1, indices.size());
        renderpass.EndRecordCmdBuffer(cmdbuffer);
        writeCmdBuffers.push_back(cmdbuffer.first);
    });

    renderpass2[0]->onRender.hook(texturePassCallback);
    renderpass2[1]->onRender.hook(texturePassCallback);

    renderpass3[0]->onRender.hook(TinyCallback<TinyRenderPass&, TinyCommandPool&, std::vector<VkCommandBuffer>&, bool>([&](TinyRenderPass& renderpass, TinyCommandPool& commandPool, std::vector<VkCommandBuffer>& writeCmdBuffers, bool frameResized) {
        camera = TinyMath::Project2D(window.hwndWidth, window.hwndHeight, 0, 0, 1.0, 0.0);

        auto cmdbuffer = renderpass.BeginRecordCmdBuffer();
            VkDescriptorImageInfo imageDescriptor = renderpass.dependencies[0]->targetImage->GetDescriptorInfo();
            VkWriteDescriptorSet imageDescriptorSet = renderpass.dependencies[0]->targetImage->GetWriteDescriptor(0, 1, &imageDescriptor);
            renderpass.PushDescriptorSet(cmdbuffer, { imageDescriptorSet });
            renderpass.PushConstants(cmdbuffer, TinyShaderStages::STAGE_VERTEX, sizeof(glm::mat4), &camera);
            renderpass.CmdBindGeometry(cmdbuffer, &vbuffer2.buffer, ibuffer.buffer);
            renderpass.CmdDrawGeometry(cmdbuffer, true, 1, indices.size());
        renderpass.EndRecordCmdBuffer(cmdbuffer);
        writeCmdBuffers.push_back(cmdbuffer.first);
    }));
    
    std::thread mythread([&window, &graph, &vkdevice]() {
        while (window.ShouldExecute()) {
            graph.RenderSwapChain();
            
            #if TINY_ENGINE_VALIDATION
                if (vkdevice.useTimestampBit)
                    for(TinyRenderPass* pass : graph.renderPasses) {
                        std::vector<float> timestamps = pass->QueryTimeStamps();
                        for(float time : timestamps)
                            std::cout << "[" << graph.frameCounter << "] " << pass->subpassIndex << " : " << pass->title << " - " << time << " ms" << std::endl;
                    }
            #endif
        }
    });
    window.WhileMain(TinyWindowEvents::WAIT_EVENTS);
    
    mythread.join();
    vkdevice.DeviceWaitIdle();
    return VK_SUCCESS;
};