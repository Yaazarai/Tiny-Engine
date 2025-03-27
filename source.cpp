#include "./TinyEngine/TinyEngine.hpp"
using namespace tny;

#define DEFAULT_TEXTURE "./Images/icons_default.qoi"
#define SPRITE_VERTEX_SHADER "./Shaders/texture_output_vert.spv"
#define SPRITE_FRAGMENT_SHADER "./Shaders/texture_output_frag.spv"
#define DEFAULT_FRAGMENT_SHADER "./Shaders/default_output_frag.spv"

int TINY_ENGINE_WINDOWMAIN {
    TinyWindow window ("Tiny Engine", 1920, 1080, true, false, true, false, true, 640, 480);
    TinyVkDevice vkdevice(true, false, true, &window);
    TinyCommandPool cmdpool(vkdevice, false);
    
    const std::tuple<TinyShaderStages, std::string> vshader_txtout = { TinyShaderStages::STAGE_VERTEX, SPRITE_VERTEX_SHADER };
    const std::tuple<TinyShaderStages, std::string> fshader_default = { TinyShaderStages::STAGE_FRAGMENT, DEFAULT_FRAGMENT_SHADER };
    const std::tuple<TinyShaderStages, std::string> fshader_txtout = { TinyShaderStages::STAGE_FRAGMENT, SPRITE_FRAGMENT_SHADER };
    const std::vector<VkPushConstantRange>& pconstant_txtout = { TinyPipeline::GetPushConstantRange(TinyShaderStages::STAGE_VERTEX, sizeof(glm::mat4)) };
    const std::vector<VkDescriptorSetLayoutBinding> playout_txtout = { TinyPipeline::GetPushDescriptorLayoutBinding(TinyShaderStages::STAGE_FRAGMENT, 0, TinyDescriptorType::TYPE_IMAGE_SAMPLER, 1) };
    
    TinyRenderShaders shaders = { { vshader_txtout, fshader_default }, pconstant_txtout, {} };
    TinyRenderShaders shaders2 = { { vshader_txtout, fshader_txtout }, pconstant_txtout, playout_txtout };
    TinyPipelineCreateInfo createInfo = TinyPipelineCreateInfo::CreateGraphicsPipeline(true, false, VK_FORMAT_B8G8R8A8_UNORM);

    TinyPipeline pipeline(vkdevice, createInfo, shaders);
    TinyPipeline pipeline2(vkdevice, createInfo, shaders2);
    TinyRenderGraph graph(vkdevice, &window);

    std::vector<TinyRenderPass*> renderpass = graph.CreateRenderPass(cmdpool, pipeline, "Texture Input Pass", { 1920, 1080 }, 1, VK_FALSE);
    std::vector<TinyRenderPass*> renderpass2 = graph.CreateRenderPass(cmdpool, pipeline2, "Copy Pass", { 1920, 1080 }, 1, VK_TRUE);
    renderpass2[0]->AddDependency(*renderpass[0]);
    
    std::vector<TinyVertex> triangles = {
        TinyVertex({0.0f,0.0f}, {240.0f,135.0f,               0.0f}, {1.0f,0.0f,0.0f,1.0f}),
        TinyVertex({0.0f,0.0f}, {240.0f+960.0f,135.0f,        0.0f}, {0.0f,1.0f,0.0f,1.0f}),
        TinyVertex({0.0f,0.0f}, {240.0f+960.0f,135.0f+540.0f, 0.0f}, {1.0f,0.0f,1.0f,1.0f}),
        TinyVertex({0.0f,0.0f}, {240.0f,135.0f + 540.0f,      0.0f}, {0.0f,0.0f,1.0f,1.0f})
    };
    std::vector<uint32_t> indices = {0,1,2,2,3,0};

    size_t sizeofTriangles = TinyMath::GetSizeofVector<TinyVertex>(triangles);
    TinyBuffer vbuffer(vkdevice, TinyBufferType::TYPE_VERTEX, sizeofTriangles);
    TinySingleSubmitCmds::StageBufferData(vbuffer, pipeline, cmdpool, triangles.data(), sizeofTriangles);
    
    size_t sizeofIndices = TinyMath::GetSizeofVector<uint32_t>(indices);
    TinyBuffer ibuffer(vkdevice, TinyBufferType::TYPE_INDEX, sizeofIndices);
    TinySingleSubmitCmds::StageBufferData(ibuffer, pipeline, cmdpool, indices.data(), sizeofIndices);

    std::vector<TinyVertex> triangles2 = TinyQuad::Create(glm::vec4(0.0, 0.0, 1920.0, 1080.0), 1.0, TinyQuad::defvcolors);
    size_t sizeofTriangles2 = TinyMath::GetSizeofVector<TinyVertex>(triangles2);
    TinyBuffer vbuffer2(vkdevice, TinyBufferType::TYPE_VERTEX, sizeofTriangles2);
    TinySingleSubmitCmds::StageBufferData(vbuffer2, pipeline2, cmdpool, triangles2.data(), sizeofTriangles2);
    
    glm::mat4 camera = TinyMath::Project2D(window.hwndWidth, window.hwndHeight, 0, 0, 1.0, 0.0);
    //TinyObject<TinyBuffer> projection = TinyBuffer::Construct(vkdevice, TinyBufferType::TYPE_UNIFORM, sizeof(glm::mat4));
    //TinySingleSubmitCmds::StageBufferData(projection, pipeline, cmdpool, &camera, sizeof(glm::mat4));
    
    //VkClearValue clearColor{ .color.float32 = { 0.0, 0.0, 0.0, 1.0 } };

    renderpass[0]->onRender.hook(TinyCallback<TinyRenderPass&, TinyCommandPool&, std::vector<VkCommandBuffer>&, bool>(
        [&camera, &indices, &vkdevice, &window, &graph, &pipeline, &vbuffer, &ibuffer/*, &projection, &clearColor*/](TinyRenderPass& renderpass, TinyCommandPool& commandPool, std::vector<VkCommandBuffer>& writeCmdBuffers, bool frameResized) {
        auto commandBuffer = commandPool.LeaseBuffer();
        
        renderpass.BeginRecordCmdBuffer(commandBuffer /*, clearColor*/);
            //VkDescriptorBufferInfo cameraDescriptorInfo = projection.ref().GetDescriptorInfo();
            //VkWriteDescriptorSet cameraDescriptor = projection.ref().GetWriteDescriptor(0, 1, { &cameraDescriptorInfo });
            //renderpass.PushDescriptorSet(commandBuffer, { cameraDescriptor });
            
            if (frameResized) 
                camera = TinyMath::Project2D(window.hwndWidth, window.hwndHeight, 0, 0, 1.0, 0.0);

            renderpass.PushConstants(commandBuffer, TinyShaderStages::STAGE_VERTEX, sizeof(glm::mat4), &camera);
            TinyRenderCmds::CmdBindGeometry(commandBuffer, &vbuffer.buffer, ibuffer.buffer);
            TinyRenderCmds::CmdDrawGeometry(commandBuffer, true, 1, indices.size());
        renderpass.EndRecordCmdBuffer(commandBuffer);
        writeCmdBuffers.push_back(commandBuffer.first);
    }));

    renderpass2[0]->onRender.hook(TinyCallback<TinyRenderPass&, TinyCommandPool&, std::vector<VkCommandBuffer>&, bool>(
        [&camera, &indices, &vkdevice, &window, &graph, &pipeline, &vbuffer2, &ibuffer/*, &projection, &clearColor*/](TinyRenderPass& renderpass, TinyCommandPool& commandPool, std::vector<VkCommandBuffer>& writeCmdBuffers, bool frameResized) {
        auto commandBuffer = commandPool.LeaseBuffer();
        
        renderpass.BeginRecordCmdBuffer(commandBuffer/*, clearColor*/);
            VkDescriptorImageInfo imageDescriptor = renderpass.dependencies[0]->targetImage->GetDescriptorInfo();
            VkWriteDescriptorSet imageDescriptorSet = renderpass.dependencies[0]->targetImage->GetWriteDescriptor(0, 1, &imageDescriptor);
            renderpass.PushDescriptorSet(commandBuffer, { imageDescriptorSet });
            
            renderpass.PushConstants(commandBuffer, TinyShaderStages::STAGE_VERTEX, sizeof(glm::mat4), &camera);
            TinyRenderCmds::CmdBindGeometry(commandBuffer, &vbuffer2.buffer, ibuffer.buffer);
            TinyRenderCmds::CmdDrawGeometry(commandBuffer, true, 1, indices.size());
        renderpass.EndRecordCmdBuffer(commandBuffer);
        writeCmdBuffers.push_back(commandBuffer.first);
    }));
    
    std::thread mythread([&window, &graph, &vkdevice]() {
        while (window.ShouldExecute())
            graph.RenderSwapChain();
            //std::cout << graph.ref().frameCounter << std::endl;
    });
    window.WhileMain(TinyWindowEvents::WAIT_EVENTS);
    mythread.join();
    vkdevice.DeviceWaitIdle();

    return VK_SUCCESS;
};