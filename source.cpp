#include "./TinyEngine/TinyEngine.hpp"
using namespace tny;

#define DEFAULT_TEXTURE "./Images/icons_default.qoi"
#define SPRITE_VERTEX_SHADER "./Shaders/texture_output_vert.spv"
#define SPRITE_FRAGMENT_SHADER "./Shaders/texture_output_frag.spv"
#define DEFAULT_FRAGMENT_SHADER "./Shaders/default_output_frag.spv"

int TINY_ENGINE_WINDOWMAIN {
    TinyObject<TinyWindow> window = TinyWindow::Construct("Tiny Engine", 1920, 1080, true, false, true, false, true, 640, 480);
    TinyObject<TinyVkDevice> vkdevice = TinyVkDevice::Construct(true, false, true, window);
    TinyObject<TinyCommandPool> cmdpool = TinyCommandPool::Construct(vkdevice, false);
    
    const std::tuple<TinyShaderStages, std::string> vshader_txtout = { TinyShaderStages::STAGE_VERTEX, SPRITE_VERTEX_SHADER };
    const std::tuple<TinyShaderStages, std::string> fshader_default = { TinyShaderStages::STAGE_FRAGMENT, DEFAULT_FRAGMENT_SHADER };
    const std::tuple<TinyShaderStages, std::string> fshader_txtout = { TinyShaderStages::STAGE_FRAGMENT, SPRITE_FRAGMENT_SHADER };
    const std::vector<VkPushConstantRange>& pconstant_txtout = { TinyPipeline::GetPushConstantRange(TinyShaderStages::STAGE_VERTEX, sizeof(glm::mat4)) };
    const std::vector<VkDescriptorSetLayoutBinding> playout_txtout = { TinyPipeline::GetPushDescriptorLayoutBinding(TinyShaderStages::STAGE_FRAGMENT, 0, TinyDescriptorType::TYPE_IMAGE_SAMPLER, 1) };
    
    TinyRenderShaders shaders = { { vshader_txtout, fshader_default }, pconstant_txtout, {} };
    TinyRenderShaders shaders2 = { { vshader_txtout, fshader_txtout }, pconstant_txtout, playout_txtout };
    TinyPipelineCreateInfo createInfo = TinyPipelineCreateInfo::CreateGraphicsPipeline(true, false, VK_FORMAT_B8G8R8A8_UNORM);

    TinyObject<TinyPipeline> pipeline = TinyPipeline::Construct(vkdevice, createInfo, shaders);
    TinyObject<TinyPipeline> pipeline2 = TinyPipeline::Construct(vkdevice, createInfo, shaders2);
    TinyObject<TinyRenderGraph> graph = TinyRenderGraph::Construct(vkdevice, window);

    std::vector<TinyRenderPass*> renderpass = graph.ref().CreateRenderPass(cmdpool, pipeline, { 1920, 1080 }, 1, VK_FALSE);
    std::vector<TinyRenderPass*> renderpass2 = graph.ref().CreateRenderPass(cmdpool, pipeline2, { 1920, 1080 }, 1, VK_TRUE);
    renderpass2[0]->AddDependency(*renderpass[0]);
    
    
    std::vector<TinyVertex> triangles = {
        TinyVertex({0.0f,0.0f}, {240.0f,135.0f,               0.0f}, {1.0f,0.0f,0.0f,1.0f}),
        TinyVertex({0.0f,0.0f}, {240.0f+960.0f,135.0f,        0.0f}, {0.0f,1.0f,0.0f,1.0f}),
        TinyVertex({0.0f,0.0f}, {240.0f+960.0f,135.0f+540.0f, 0.0f}, {1.0f,0.0f,1.0f,1.0f}),
        TinyVertex({0.0f,0.0f}, {240.0f,135.0f + 540.0f,      0.0f}, {0.0f,0.0f,1.0f,1.0f})
    };
    std::vector<uint32_t> indices = {0,1,2,2,3,0};

    size_t sizeofTriangles = TinyMath::GetSizeofVector<TinyVertex>(triangles);
    TinyObject<TinyBuffer> vbuffer = TinyBuffer::Construct(vkdevice, TinyBufferType::TYPE_VERTEX, sizeofTriangles);
    TinySingleSubmitCmds::StageBufferData(vbuffer, pipeline, cmdpool, triangles.data(), sizeofTriangles);
    
    size_t sizeofIndices = TinyMath::GetSizeofVector<uint32_t>(indices);
    TinyObject<TinyBuffer> ibuffer = TinyBuffer::Construct(vkdevice, TinyBufferType::TYPE_INDEX, sizeofIndices);
    TinySingleSubmitCmds::StageBufferData(ibuffer, pipeline, cmdpool, indices.data(), sizeofIndices);

    std::vector<TinyVertex> triangles2 = TinyQuad::Create(glm::vec4(0.0, 0.0, 1920.0, 1080.0), 1.0, TinyQuad::defvcolors);
    size_t sizeofTriangles2 = TinyMath::GetSizeofVector<TinyVertex>(triangles2);
    TinyObject<TinyBuffer> vbuffer2 = TinyBuffer::Construct(vkdevice, TinyBufferType::TYPE_VERTEX, sizeofTriangles2);
    TinySingleSubmitCmds::StageBufferData(vbuffer2, pipeline2, cmdpool, triangles2.data(), sizeofTriangles2);
    
    glm::mat4 camera = TinyMath::Project2D(window.ref().hwndWidth, window.ref().hwndHeight, 0, 0, 1.0, 0.0);
    //TinyObject<TinyBuffer> projection = TinyBuffer::Construct(vkdevice, TinyBufferType::TYPE_UNIFORM, sizeof(glm::mat4));
    //TinySingleSubmitCmds::StageBufferData(projection, pipeline, cmdpool, &camera, sizeof(glm::mat4));

    VkClearValue clearColor{ .color.float32 = { 0.0, 0.0, 0.0, 1.0 } };

    renderpass[0]->onRender.hook(TinyCallback<TinyRenderPass&, TinyCommandPool&, std::vector<VkCommandBuffer>&>(
        [&camera, &indices, &vkdevice, &window, &graph, &pipeline, &vbuffer, &ibuffer, /*&projection,*/ &clearColor](TinyRenderPass& renderpass, TinyCommandPool& commandPool, std::vector<VkCommandBuffer>& writeCmdBuffers) {
        auto commandBuffer = commandPool.LeaseBuffer();
        
        renderpass.BeginRecordCmdBuffer(commandBuffer.first, clearColor);        
            //VkDescriptorBufferInfo cameraDescriptorInfo = projection.ref().GetDescriptorInfo();
            //VkWriteDescriptorSet cameraDescriptor = projection.ref().GetWriteDescriptor(0, 1, { &cameraDescriptorInfo });
            //renderpass.PushDescriptorSet(commandBuffer.first, { cameraDescriptor });
            
            renderpass.PushConstants(commandBuffer.first, TinyShaderStages::STAGE_VERTEX, sizeof(glm::mat4), &camera);
            TinyRenderCmds::CmdBindGeometry(commandBuffer.first, &vbuffer.ref().buffer, ibuffer.ref().buffer);
            TinyRenderCmds::CmdDrawGeometry(commandBuffer.first, true, 1, indices.size());
        renderpass.EndRecordCmdBuffer(commandBuffer.first, clearColor);
        writeCmdBuffers.push_back(commandBuffer.first);
    }));

    renderpass2[0]->onRender.hook(TinyCallback<TinyRenderPass&, TinyCommandPool&, std::vector<VkCommandBuffer>&>(
        [&camera, &indices, &vkdevice, &window, &graph, &pipeline, &vbuffer2, &ibuffer, /*&projection,*/ &clearColor](TinyRenderPass& renderpass, TinyCommandPool& commandPool, std::vector<VkCommandBuffer>& writeCmdBuffers) {
        auto commandBuffer = commandPool.LeaseBuffer();
        
        renderpass.BeginRecordCmdBuffer(commandBuffer.first, clearColor);        
            //VkDescriptorBufferInfo cameraDescriptorInfo = projection.ref().GetDescriptorInfo();
            //VkWriteDescriptorSet cameraDescriptor = projection.ref().GetWriteDescriptor(0, 1, { &cameraDescriptorInfo });
            //renderpass.PushDescriptorSet(commandBuffer.first, { cameraDescriptor });

            VkDescriptorImageInfo imageDescriptor = renderpass.dependencies[0]->targetImage->GetDescriptorInfo();
            VkWriteDescriptorSet imageDescriptorSet = renderpass.dependencies[0]->targetImage->GetWriteDescriptor(0, 1, &imageDescriptor);
            renderpass.PushDescriptorSet(commandBuffer.first, { imageDescriptorSet });
            
            renderpass.PushConstants(commandBuffer.first, TinyShaderStages::STAGE_VERTEX, sizeof(glm::mat4), &camera);
            TinyRenderCmds::CmdBindGeometry(commandBuffer.first, &vbuffer2.ref().buffer, ibuffer.ref().buffer);
            TinyRenderCmds::CmdDrawGeometry(commandBuffer.first, true, 1, indices.size());
        renderpass.EndRecordCmdBuffer(commandBuffer.first, clearColor);
        writeCmdBuffers.push_back(commandBuffer.first);
    }));
    
    std::thread mythread([&window, &graph, &vkdevice]() {
        while (window.ref().ShouldExecute())
            graph.ref().RenderSwapChain();
            //std::cout << graph.ref().frameCounter << std::endl;
    });
    window.ref().WhileMain(TinyWindowEvents::WAIT_EVENTS);
    mythread.join();
    vkdevice.ref().DeviceWaitIdle();

    return VK_SUCCESS;
};