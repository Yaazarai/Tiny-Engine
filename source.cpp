#include "./TinyEngine/TinyEngine.hpp"
using namespace tny;

#define DEFAULT_VERTEX_SHADER "./Shaders/passthrough_vert.spv"
#define DEFAULT_FRAGMENT_SHADER "./Shaders/passthrough_frag.spv"
const std::tuple<VkShaderStageFlagBits, std::string> vertexShader = { VK_SHADER_STAGE_VERTEX_BIT, DEFAULT_VERTEX_SHADER };
const std::tuple<VkShaderStageFlagBits, std::string> fragmentShader = { VK_SHADER_STAGE_FRAGMENT_BIT, DEFAULT_FRAGMENT_SHADER };
const std::vector<std::tuple<VkShaderStageFlagBits, std::string>> defaultShaders = { vertexShader, fragmentShader };
const TinyVertexDescription vertexDescription = TinyVertex::GetVertexDescription();
const std::vector<VkDescriptorSetLayoutBinding> pushDescriptorLayouts = { TinyGraphicsPipeline::SelectPushDescriptorLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1) };

int32_t TINY_WINDOWMAIN {
    TinyConstruct<TinyWindow> window = TinyWindow::Construct("Tiny Engine", 640, 480, true, false, true, 640, 480);
    TinyConstruct<TinyVkDevice> vkdevice = TinyVkDevice::Construct(true, false, true, window);
    TinyConstruct<TinyCommandPool> cmdpool = TinyCommandPool::Construct(vkdevice, false);
    TinyConstruct<TinyGraphicsPipeline> pipeline = TinyGraphicsPipeline::Construct(vkdevice, vertexDescription, defaultShaders, pushDescriptorLayouts, {}, false);
    TinyConstruct<TinyRenderContext> context = TinyRenderContext::Construct(vkdevice, cmdpool, pipeline);
    TinyConstruct<TinySwapchain> swapchain = TinySwapchain::Construct(context, window, TinyBufferingMode::MODE_DOUBLE);

    std::vector<TinyVertex> triangles = {
        TinyVertex({0.0f,0.0f}, {240.0f,135.0f,               1.0f}, {1.0f,0.0f,0.0f,1.0f}),
        TinyVertex({0.0f,0.0f}, {240.0f+960.0f,135.0f,        1.0f}, {0.0f,1.0f,0.0f,1.0f}),
        TinyVertex({0.0f,0.0f}, {240.0f+960.0f,135.0f+540.0f, 1.0f}, {1.0f,0.0f,1.0f,1.0f}),
        TinyVertex({0.0f,0.0f}, {240.0f,135.0f + 540.0f,      1.0f}, {0.0f,0.0f,1.0f,1.0f})
    };
    //std::vector<TinyVertex> triangles = TinyQuad::CreateWithOffsetExt(glm::vec2(960.0f/2.0, 540.0f/2.0), glm::vec3(960.0f, 540.0f, 0.0f));
    std::vector<uint32_t> indices = {0,1,2,2,3,0};
    size_t sizeofTriangles = TinyMath::GetSizeofVector<TinyVertex>(triangles);
    size_t sizeofIndices = TinyMath::GetSizeofVector<uint32_t>(indices);

    TinyConstruct<TinyBuffer> vbuffer = TinyBuffer::Construct(context, TinyBufferType::TYPE_VERTEX, sizeofTriangles);
    TinyConstruct<TinyBuffer> ibuffer = TinyBuffer::Construct(context, TinyBufferType::TYPE_INDEX, sizeofIndices);
    vbuffer.ref().StageBufferData(triangles.data(), sizeofTriangles);
    ibuffer.ref().StageBufferData(indices.data(), sizeofIndices);

    TinyConstruct<TinyBuffer> projection1 = TinyBuffer::Construct(context, TinyBufferType::TYPE_UNIFORM, sizeof(glm::mat4));
    TinyConstruct<TinyBuffer> projection2 = TinyBuffer::Construct(context, TinyBufferType::TYPE_UNIFORM, sizeof(glm::mat4));

    struct SwapFrame {
        TinyBuffer &projection;
        SwapFrame(TinyBuffer& projection) : projection(projection) {}
    };

    TinyResourceQueue<SwapFrame,static_cast<size_t>(TinyBufferingMode::MODE_DOUBLE)> queue(
        { SwapFrame(projection1), SwapFrame(projection2) },
        TinyCallback<size_t&>([&swapchain](size_t& frameIndex){ frameIndex = swapchain.ref().GetSyncronizedFrameIndex(); }),
        TinyCallback<SwapFrame&>([](SwapFrame& resource){})
    );

    VkClearValue clearColor{ .color.float32 = { 0.0, 0.0, 0.5, 1.0 } };
    VkClearValue depthStencil{ .depthStencil = { 1.0f, 0 } };
    
    // TESTING RENDERCONTEXT CHANGES WITH THE SWAPCHAIN RENDERER.
    int angle = 0;
    swapchain.ref().onRenderEvents.hook(TinyCallback<TinyCommandPool&>(
        [&angle, &indices, &vkdevice, &window, &swapchain, &pipeline, &queue, &vbuffer, &ibuffer, &clearColor](TinyCommandPool& commandPool) {
        auto frame = queue.GetFrameResource();

        auto commandBuffer = commandPool.LeaseBuffer();
        swapchain.ref().BeginRecordCmdBuffer(commandBuffer.first);
        
            int offsetx = 0, offsety = 0;
            //offsetx = glm::sin(glm::radians(static_cast<glm::float32>(angle))) * 64;
            //offsety = glm::cos(glm::radians(static_cast<glm::float32>(angle))) * 64;

            //offsetx = glfwGetGamepadAxis(TinyGamepads::GPAD_01,TinyGamepadAxis::AXIS_LEFTX) * 64;
            //offsety = glfwGetGamepadAxis(TinyGamepads::GPAD_01,TinyGamepadAxis::AXIS_LEFTY) * 64;

            double mousex, mousey;
            glfwGetCursorPos(window.ref().hwndWindow, &mousex, &mousey);
            int leftclick = glfwGetMouseButton(window.ref().hwndWindow, GLFW_MOUSE_BUTTON_1);
            offsetx = int(mousex) * leftclick;
            offsety = int(mousey) * leftclick;

            glm::mat4 camera = TinyMath::Project2D(window.ref().hwndWidth, window.ref().hwndHeight, offsetx, offsety, 1.0, 0.0);
            frame.projection.StageBufferData(&camera, sizeof(glm::mat4), 0, 0);
            VkDescriptorBufferInfo cameraDescriptorInfo = frame.projection.GetBufferDescriptor();
            VkWriteDescriptorSet cameraDescriptor = pipeline.ref().SelectWriteBufferDescriptor(0, 1, { &cameraDescriptorInfo });
            swapchain.ref().PushDescriptorSet(commandBuffer.first, { cameraDescriptor });
            
            VkDeviceSize offsets[] = { 0 };
            swapchain.ref().CmdBindGeometry(commandBuffer.first, &vbuffer.ref().buffer, ibuffer.ref().buffer, offsets);
            swapchain.ref().CmdDrawGeometry(commandBuffer.first, true, 1, 0, indices.size(), 0, 0);    
        
        swapchain.ref().EndRecordCmdBuffer(commandBuffer.first);
        angle += 1;

        //if (angle % 200 == 0) swapRenderer.PushPresentMode(VK_PRESENT_MODE_IMMEDIATE_KHR);
        //if (angle % 400 == 0) swapRenderer.PushPresentMode(VK_PRESENT_MODE_FIFO_KHR);
    }));

    // MULTI-THREADED: (window events on main, rendering on secondary)
    std::thread mythread([&window, &swapchain]() { while (window.ref().ShouldExecute()) {
        VkResult result = swapchain.ref().RenderExecute();
    } });
    window.ref().WhileMain(TinyWindowEvents::WAIT_EVENTS);
    mythread.join();
    return VK_SUCCESS;
}