#include "./TinyEngine/TinyEngine.hpp"
using namespace tny;

#define DEFAULT_TEXTURE "./Images/icons_default.qoi"
#define SPRITE_VERTEX_SHADER "./Shaders/texture_output_vert.spv"
#define SPRITE_FRAGMENT_SHADER "./Shaders/texture_output_frag.spv"

void SwapchainRenderer(TinyRenderContext& context, TinyWindow& window);

int TINY_ENGINE_WINDOWMAIN {
    TinyObject<TinyWindow> window = TinyWindow::Construct("Tiny Engine", 1920, 1080, true, false, true, false, true, 1920, 1080);
    TinyObject<TinyVkDevice> vkdevice = TinyVkDevice::Construct(true, false, true, window);
    TinyObject<TinyCommandPool> cmdpool = TinyCommandPool::Construct(vkdevice, false);
    
    const std::tuple<TinyShaderStages, std::string> vshader_txtout = { TinyShaderStages::STAGE_VERTEX, SPRITE_VERTEX_SHADER };
    const std::tuple<TinyShaderStages, std::string> fshader_txtout = { TinyShaderStages::STAGE_FRAGMENT, SPRITE_FRAGMENT_SHADER };
    const std::vector<VkPushConstantRange>& pconstant_txtout = { TinyGraphicsPipeline::SelectPushConstantRange(sizeof(glm::mat4), TinyShaderStages::STAGE_VERTEX) };
    const std::vector<VkDescriptorSetLayoutBinding> playout_txtout = { TinyGraphicsPipeline::SelectPushDescriptorLayoutBinding(1, TinyDescriptorType::TYPE_IMAGE_SAMPLER, TinyShaderStages::STAGE_FRAGMENT, 1) };
    
    TinyObject<TinyGraphicsPipeline> pipeline = TinyGraphicsPipeline::Construct(vkdevice, TinyVertex::GetVertexDescription(), { vshader_txtout, fshader_txtout }, playout_txtout, pconstant_txtout, false);
    TinyObject<TinyRenderContext> context = TinyRenderContext::Construct(vkdevice, cmdpool, pipeline);
    
    SwapchainRenderer(context, window);
    return VK_SUCCESS;
};

void SwapchainRenderer(TinyRenderContext& context, TinyWindow& window) {
    TinyObject<TinySwapchain> swapchain = TinySwapchain::Construct(&context, window, TinyBufferingMode::MODE_DOUBLE);
    
    const std::tuple<TinyShaderStages, std::string> vshader_txtout = { TinyShaderStages::STAGE_VERTEX, SPRITE_VERTEX_SHADER };
    const std::tuple<TinyShaderStages, std::string> fshader_txtout = { TinyShaderStages::STAGE_FRAGMENT, SPRITE_FRAGMENT_SHADER };
    const std::vector<VkPushConstantRange>& pconstant_txtout = { TinyGraphicsPipeline::SelectPushConstantRange(sizeof(glm::mat4), TinyShaderStages::STAGE_VERTEX) };
    const std::vector<VkDescriptorSetLayoutBinding> playout_txtout = { TinyGraphicsPipeline::SelectPushDescriptorLayoutBinding(1, TinyDescriptorType::TYPE_IMAGE_SAMPLER, TinyShaderStages::STAGE_FRAGMENT, 1) };
    TinyObject<TinyGraphicsPipeline> pipelineRC = TinyGraphicsPipeline::Construct(context.vkdevice, TinyVertex::GetVertexDescription(), { vshader_txtout, fshader_txtout }, playout_txtout, pconstant_txtout, false);
    TinyObject<TinyRenderContext> contextRC = TinyRenderContext::Construct(context.vkdevice, context.commandPool, pipelineRC);
    TinyObject<TinyRenderer> rendererRC = TinyRenderer::Construct(contextRC.ptr(), &context.commandPool, nullptr, nullptr);

    TinyObject<TinyImage> outputTexture1 = TinyImage::Construct(context, TinyImageType::TYPE_COLORATTACHMENT, window.hwndWidth, window.hwndHeight);
    TinyObject<TinyImage> outputTexture2 = TinyImage::Construct(context, TinyImageType::TYPE_COLORATTACHMENT, window.hwndWidth, window.hwndHeight);
    
    qoi_desc texture_sheet;
    void* texture_data = qoi_read(DEFAULT_TEXTURE, &texture_sheet, 0);
    int width, height, channels;
    width = texture_sheet.width;
    height = texture_sheet.height;
    channels = texture_sheet.channels;
    
    // START IN COLOR_ATTACHEMENT LAYOUT FOR CPU-SIDE WRITE, THEN TRANSITION TO SHADER_READONLY FOR FRAGMENT SHADER.
    TinyObject<TinyImage> texture = TinyImage::Construct(context, TinyImageType::TYPE_COLORATTACHMENT, width, height);
    texture.ref().StageImageData(texture_data, width * height * channels);
    texture.ref().TransitionLayoutCmd(TinyImageLayout::LAYOUT_SHADER_READONLY);

    struct SwapFrame {
        TinyImage& outputTexture;
        SwapFrame(TinyImage& outputTexture) : outputTexture(outputTexture) {}
    };

    TinyResourceQueue<SwapFrame,static_cast<size_t>(TinyBufferingMode::MODE_DOUBLE)> frameQueue(
        { SwapFrame(outputTexture1.ref()), SwapFrame(outputTexture2.ref()) },
        TinyCallback<size_t&>([&](size_t& frameIndex){ frameIndex = swapchain.ref().GetSyncronizedFrameIndex(); }),
        TinyCallback<SwapFrame&>([](SwapFrame& resource){})
    );

    std::vector<TinyVertex> triangles = TinyQuad::Create(glm::vec4(0.0, 0.0, window.hwndWidth, window.hwndHeight), 1.0, TinyQuad::defvcolors);
    std::vector<uint32_t> indices = {0,1,2,2,3,0};
    size_t sizeofTriangles = TinyMath::GetSizeofVector<TinyVertex>(triangles);
    size_t sizeofIndices = TinyMath::GetSizeofVector<uint32_t>(indices);
    TinyObject<TinyBuffer> vbuffer = TinyBuffer::Construct(context, TinyBufferType::TYPE_VERTEX, sizeofTriangles);
    TinyObject<TinyBuffer> ibuffer = TinyBuffer::Construct(context, TinyBufferType::TYPE_INDEX, sizeofIndices);
    vbuffer.ref().StageBufferData(triangles.data(), sizeofTriangles);
    ibuffer.ref().StageBufferData(indices.data(), sizeofIndices);
    
    /*
        Creates a standalone render event.
    */
    rendererRC.ref().onRenderEvents.hook(TinyCallback<TinyCommandPool&>(
    [&](TinyCommandPool& commandPool) {
        auto cmdBuffer = commandPool.LeaseBuffer();
        auto frame = frameQueue.GetFrameResource();
        rendererRC.ref().BeginRecordCmdBuffer(cmdBuffer.first);
            glm::mat4 projection = TinyMath::Project2D(window.hwndWidth, window.hwndHeight, 0.0, 0.0, 1.0, 0.0);
            rendererRC.ref().PushConstants(cmdBuffer.first, TinyShaderStages::STAGE_VERTEX, sizeof(glm::mat4), &projection);

            VkDescriptorImageInfo textureDescriptorInfo = texture.ref().GetImageDescriptor();
            VkWriteDescriptorSet textureDescriptor = context.graphicsPipeline.SelectWriteImageDescriptor(1, 1, { &textureDescriptorInfo });

            rendererRC.ref().PushDescriptorSet(cmdBuffer.first, { textureDescriptor });
            rendererRC.ref().CmdBindGeometry(cmdBuffer.first, &vbuffer.ref().buffer, ibuffer.ref().buffer);
            swapchain.ref().CmdDrawGeometry(cmdBuffer.first, true, 1, indices.size());
        rendererRC.ref().EndRecordCmdBuffer(cmdBuffer.first);
    }));
    
    /*
        Renders an outpuit texture from the render pass to the screen.
            * Waits (using CPU-GPU sync) on the sub-render pass for the output texture.
    */
    swapchain.ref().onRenderEvents.hook(TinyCallback<TinyCommandPool&>(
    [&](TinyCommandPool& commandPool) {
        auto cmdBuffer = commandPool.LeaseBuffer();
        auto frame = frameQueue.GetFrameResource();

        frame.outputTexture.TransitionLayoutCmd(TinyImageLayout::LAYOUT_COLOR_ATTACHMENT);
        rendererRC.ref().SetRenderTarget(contextRC, &contextRC.ref().commandPool, &frame.outputTexture, nullptr, false);
        rendererRC.ref().RenderExecute();
        vkWaitForFences(context.vkdevice.logicalDevice, 1, &frame.outputTexture.imageWaitable, true, UINT64_MAX);
        frame.outputTexture.TransitionLayoutCmd(TinyImageLayout::LAYOUT_SHADER_READONLY);

        swapchain.ref().BeginRecordCmdBuffer(cmdBuffer.first);
            double mousex, mousey;
            glfwGetCursorPos(window.hwndWindow, &mousex, &mousey);
            int leftclick = glfwGetMouseButton(window.hwndWindow, GLFW_MOUSE_BUTTON_1);
            mousex *= (double)leftclick;
            mousey *= (double)leftclick;
            
            glm::mat4 projection = TinyMath::Project2D(window.hwndWidth, window.hwndHeight, mousex, mousey, 1.0, 0.0);
            swapchain.ref().PushConstants(cmdBuffer.first, TinyShaderStages::STAGE_VERTEX, sizeof(glm::mat4), &projection);

            VkDescriptorImageInfo textureDescriptorInfo = frame.outputTexture.GetImageDescriptor();
            VkWriteDescriptorSet textureDescriptor = context.graphicsPipeline.SelectWriteImageDescriptor(1, 1, { &textureDescriptorInfo });

            swapchain.ref().PushDescriptorSet(cmdBuffer.first, { textureDescriptor });
            swapchain.ref().CmdBindGeometry(cmdBuffer.first, &vbuffer.ref().buffer, ibuffer.ref().buffer);
            swapchain.ref().CmdDrawGeometry(cmdBuffer.first, true, 1, indices.size());
        swapchain.ref().EndRecordCmdBuffer(cmdBuffer.first);
    }));
    
    std::thread mythread([&]() { while (context.vkdevice.window->ShouldExecute()) swapchain.ref().RenderExecute(); });
    context.vkdevice.window->WhileMain(TinyWindowEvents::WAIT_EVENTS);
    mythread.join();
}