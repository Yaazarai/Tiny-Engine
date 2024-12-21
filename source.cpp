#include "./TinyEngine/TinyEngine.hpp"
using namespace tny;

#define DEFAULT_TEXTURE "./Images/icons_default.qoi"
//#define DEFAULT_VERTEX_SHADER "./Shaders/passthrough_vert.spv"
//#define DEFAULT_FRAGMENT_SHADER "./Shaders/passthrough_frag.spv"
#define SPRITE_VERTEX_SHADER "./Shaders/sprite_render_vert.spv"
#define SPRITE_FRAGMENT_SHADER "./Shaders/sprite_render_frag.spv"
const std::tuple<TinyShaderStages, std::string> vertexShader = { TinyShaderStages::STAGE_VERTEX, SPRITE_VERTEX_SHADER };
const std::tuple<TinyShaderStages, std::string> fragmentShader = { TinyShaderStages::STAGE_FRAGMENT, SPRITE_FRAGMENT_SHADER };
const std::vector<std::tuple<TinyShaderStages, std::string>> defaultShaders = { vertexShader, fragmentShader };
const TinyVertexDescription vertexDescription = TinyVertex::GetVertexDescription();

const std::vector<VkPushConstantRange>& pushConstantRanges = { TinyGraphicsPipeline::SelectPushConstantRange(sizeof(glm::mat4), TinyShaderStages::STAGE_VERTEX) };
const std::vector<VkDescriptorSetLayoutBinding> pushDescriptorLayouts = {
    TinyGraphicsPipeline::SelectPushDescriptorLayoutBinding(0, TinyDescriptorType::TYPE_UNIFORM_BUFFER, TinyShaderStages::STAGE_VERTEX, 1),
    TinyGraphicsPipeline::SelectPushDescriptorLayoutBinding(1, TinyDescriptorType::TYPE_IMAGE_SAMPLER, TinyShaderStages::STAGE_FRAGMENT, 1)
};

int TINY_ENGINE_WINDOWMAIN {
    TinyObject<TinyWindow> window = TinyWindow::Construct("Tiny Engine", 1920, 1080, true, false, true, false, true, 640, 480);
    TinyObject<TinyVkDevice> vkdevice = TinyVkDevice::Construct(true, false, true, window);
    TinyObject<TinyCommandPool> cmdpool = TinyCommandPool::Construct(vkdevice, false);
    TinyObject<TinyGraphicsPipeline> pipeline = TinyGraphicsPipeline::Construct(vkdevice, vertexDescription, defaultShaders, pushDescriptorLayouts, pushConstantRanges, false);
    TinyObject<TinyRenderContext> context = TinyRenderContext::Construct(vkdevice, cmdpool, pipeline);
    TinyObject<TinySwapchain> swapchain = TinySwapchain::Construct(context, window, TinyBufferingMode::MODE_DOUBLE);
    //swapchain.ref().PushPresentMode(VK_PRESENT_MODE_FIFO_KHR);
    
    qoi_desc texture_sheet;
    void* texture_data = qoi_read(DEFAULT_TEXTURE, &texture_sheet, 0);
    int width, height, channels;
    width = texture_sheet.width;
    height = texture_sheet.height;
    channels = texture_sheet.channels;
    
    // START IN COLOR_ATTACHEMENT LAYOUT FOR CPU-SIDE WRITE, THEN TRANSITION TO SHADER_READONLY FOR FRAGMENT SHADER.
    TinyObject<TinyImage> texture = TinyImage::Construct(context.ref(), TinyImageType::TYPE_COLORATTACHMENT, width, height);
    texture.ref().StageImageData(texture_data, width * height * channels);
    texture.ref().TransitionLayoutCmd(TinyImageLayout::LAYOUT_SHADER_READONLY);

    //std::vector<TinyVertex> triangles = {
    //    TinyVertex({0.0f,0.0f}, {240.0f,135.0f,               1.0f}, {1.0f,0.0f,00.0f,1.0f}),
    //    TinyVertex({0.0f,0.0f}, {240.0f+960.0f,135.0f,        1.0f}, {0.0f,1.0f,0.0f,1.0f}),
    //    TinyVertex({0.0f,0.0f}, {240.0f+960.0f,135.0f+540.0f, 1.0f}, {1.0f,1.0f,1.0f,1.0f}),
    //    TinyVertex({0.0f,0.0f}, {240.0f,135.0f + 540.0f,      1.0f}, {0.0f,0.0f,1.0f,1.0f})
    //};
    std::vector<glm::vec4> vcolors = TinyQuad::CreateVertexColors({1.0f,0.0f,00.0f,1.0f}, {0.0f,1.0f,0.0f,1.0f}, {0.0f,0.0f,1.0f,1.0f}, {1.0f,1.0f,1.0f,1.0f});
    //std::vector<TinyVertex> triangles = TinyQuad::Create(glm::vec4(240.0,135.0,960.0,540.0), 1.0, vcolors);
    std::vector<TinyVertex> triangles1 = TinyQuad::CreateFromAtlas(glm::vec4(0.0, 0.0, width/2, height/2), 1.0, glm::vec4(0.0, 0.0, floor((width/2)/64)*64, floor((height/2)/64)*64), glm::vec2(width, height), vcolors);
    std::vector<TinyVertex> triangles2 = TinyQuad::CreateFromAtlas(glm::vec4(0.0, 512.0, width/4, height/4), 1.0, glm::vec4(0.0, 0.0, floor((width/4)/64)*64, floor((height/4)/64)*64), glm::vec2(width, height), vcolors);
    std::vector<uint32_t> indices1 = {0,1,2,2,3,0};
    std::vector<uint32_t> indices2 = {4,5,6,6,7,4};
    size_t sizeofTriangles = TinyMath::GetSizeofVector<TinyVertex>(triangles1) * 2;
    size_t sizeofIndices = TinyMath::GetSizeofVector<uint32_t>(indices1) * 2;

    TinyObject<TinyBuffer> vbuffer = TinyBuffer::Construct(context.ref(), TinyBufferType::TYPE_VERTEX, sizeofTriangles);
    TinyObject<TinyBuffer> ibuffer = TinyBuffer::Construct(context.ref(), TinyBufferType::TYPE_INDEX, sizeofIndices);
    
    std::vector<TinyVertex> triangles;
    std::vector<uint32_t> indices;
    
    for(auto vv : triangles1)
        triangles.push_back(vv);
    for(auto vv : triangles2)
        triangles.push_back(vv);
    for(auto ii : indices1)
        indices.push_back(ii);
    for(auto ii : indices2)
        indices.push_back(ii);

    vbuffer.ref().StageBufferData(triangles.data(), sizeofTriangles);
    ibuffer.ref().StageBufferData(indices.data(), sizeofIndices);

    TinyObject<TinyBuffer> projection1 = TinyBuffer::Construct(context.ref(), TinyBufferType::TYPE_UNIFORM, sizeof(glm::mat4));
    TinyObject<TinyBuffer> projection2 = TinyBuffer::Construct(context.ref(), TinyBufferType::TYPE_UNIFORM, sizeof(glm::mat4));

    struct SwapFrame {
        TinyBuffer &projection;
        SwapFrame(TinyBuffer& projection) : projection(projection) {}
    };

    TinyResourceQueue<SwapFrame,static_cast<size_t>(TinyBufferingMode::MODE_DOUBLE)> queue(
        { SwapFrame(projection1.ref()), SwapFrame(projection2.ref()) },
        TinyCallback<size_t&>([&swapchain](size_t& frameIndex){ frameIndex = swapchain.ref().GetSyncronizedFrameIndex(); }),
        TinyCallback<SwapFrame&>([](SwapFrame& resource){})
    );

    VkClearValue clearColor{ .color.float32 = { 0.0, 0.0, 0.5, 1.0 } };
    VkClearValue depthStencil{ .depthStencil = { 1.0f, 0 } };

    //std::cout << "Set Target Check Error: " << swapchain.ref().SetRenderTarget(context, cmdpool, swapchain.ref().imageSources[0], VK_NULL_HANDLE, false) << std::endl;
    
    // TESTING RENDERCONTEXT CHANGES WITH THE SWAPCHAIN RENDERER.
    float current_time, previous_time;
    int angle = 0;
    swapchain.ref().onRenderEvents.hook(TinyCallback<TinyCommandPool&>(
        [&current_time, &previous_time, &angle, &texture, &triangles, &indices, &vkdevice, &window, &swapchain, &pipeline, &queue, &vbuffer, &ibuffer, &clearColor](TinyCommandPool& commandPool) {
        auto frame = queue.GetFrameResource();

        auto commandBuffer = commandPool.LeaseBuffer();
        swapchain.ref().BeginRecordCmdBuffer(commandBuffer.first);
        
            int offsetx = 0, offsety = 0;
            offsetx = glm::sin(glm::radians(static_cast<glm::float32>(angle))) * 96;
            offsety = glm::cos(glm::radians(static_cast<glm::float32>(angle))) * 96;

            //offsetx = glfwGetGamepadAxis(TinyGamepads::GPAD_01,TinyGamepadAxis::AXIS_LEFTX) * 64;
            //offsety = glfwGetGamepadAxis(TinyGamepads::GPAD_01,TinyGamepadAxis::AXIS_LEFTY) * 64;

            double mousex, mousey;
            glfwGetCursorPos(window.ref().hwndWindow, &mousex, &mousey);
            int leftclick = glfwGetMouseButton(window.ref().hwndWindow, GLFW_MOUSE_BUTTON_1);
            glm::vec4 sizes = TinyQuad::GetQuadXYWH(triangles);
            offsetx += (int(mousex) * leftclick);
            offsety += (int(mousey) * leftclick);

            glm::mat4 camera = TinyMath::Project2D(window.ref().hwndWidth, window.ref().hwndHeight, offsetx, offsety, 1.0, 0.0);
            frame.projection.StageBufferData(&camera, sizeof(glm::mat4), 0, 0);
            glm::mat4 projection = TinyMath::Project2D(window.ref().hwndWidth, window.ref().hwndHeight, 0.0, 0.0, 1.0, 0.0);
            
            //swapchain.ref().PushConstants(commandBuffer.first, VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), &projection);
            VkDescriptorBufferInfo cameraDescriptorInfo = frame.projection.GetBufferDescriptor();
            VkWriteDescriptorSet cameraDescriptor = pipeline.ref().SelectWriteBufferDescriptor(0, 1, { &cameraDescriptorInfo });
            
            VkDescriptorImageInfo textureDescriptorInfo = texture.ref().GetImageDescriptor();
            VkWriteDescriptorSet textureDescriptor = pipeline.ref().SelectWriteImageDescriptor(1, 1, { &textureDescriptorInfo });

            swapchain.ref().PushDescriptorSet(commandBuffer.first, { cameraDescriptor, textureDescriptor });
            
            //VkDeviceSize offsets[] = { 0 };
            //uint32_t bindingCount = pipeline.ref().SelectBindingCountByShaderStage(pipeline.ref(), VK_SHADER_STAGE_VERTEX_BIT);
            //vkCmdBindVertexBuffers(commandBuffer.first, 0, bindingCount, &vbuffer.ref().buffer, offsets);
            //vkCmdBindIndexBuffer(commandBuffer.first, ibuffer.ref().buffer, 0, VK_INDEX_TYPE_UINT32);
            //vkCmdDrawIndexed(commandBuffer.first, 12, 2, 0, 0, 0);

            swapchain.ref().CmdBindGeometry(commandBuffer.first, &vbuffer.ref().buffer, ibuffer.ref().buffer);
            swapchain.ref().CmdDrawGeometry(commandBuffer.first, true, 2, indices.size());
        
        swapchain.ref().EndRecordCmdBuffer(commandBuffer.first);

        std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch());
        current_time = (float) ms.count();
        
        angle ++;

        if (current_time >= previous_time + 1000.0) {
            std::cout << "Frame Count: " << angle << std::endl;
            previous_time = current_time;
            angle = 0;
        }

        //if (angle % 200 == 0) swapchain.ref().PushPresentMode(VK_PRESENT_MODE_IMMEDIATE_KHR);
        //if (angle % 400 == 0) swapchain.ref().PushPresentMode(VK_PRESENT_MODE_FIFO_KHR);
    }));

    // MULTI-THREADED: (window events on main, rendering on secondary)
    std::thread mythread([&window, &swapchain]() { while (window.ref().ShouldExecute()) {
        VkResult result = swapchain.ref().RenderExecute();
    }});
    window.ref().WhileMain(TinyWindowEvents::WAIT_EVENTS);
    mythread.join();
    
    //window.ref().onWhileMain.hook(TinyCallback<std::atomic_bool&>([&swapchain](std::atomic_bool& flag) {
    //    VkResult result = swapchain.ref().RenderExecute();
    //}));
    //window.ref().WhileMain(TinyWindowEvents::POLL_EVENTS);
    return VK_SUCCESS;
};