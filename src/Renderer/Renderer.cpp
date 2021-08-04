#include "Renderer.h"

namespace SnekVk
{
    Utils::Array<VkCommandBuffer> Renderer::commandBuffers;

    Renderer::Renderer(SnekVk::Window& window) 
        : window{window}, 
        device{VulkanDevice(window)}, 
        swapChain{SwapChain(device,  
        window.GetExtent())}
    {
        // Currently, we're just allocating 100 possible models.
        models.reserve(100);
        CreateCommandBuffers();
    }
    
    Renderer::~Renderer() 
    {
        vkDestroyPipelineLayout(device.Device(), pipelineLayout, nullptr);
    }

    void Renderer::SubmitModel(Model* model)
    {
        models.emplace_back(model);
    }

    void Renderer::CreateCommandBuffers()
    {
        commandBuffers = Utils::Array<VkCommandBuffer>(swapChain.GetImageCount());
        u32 size = swapChain.GetImageCount();

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = device.GetCommandPool();
        allocInfo.commandBufferCount = size;

        SNEK_ASSERT(vkAllocateCommandBuffers(device.Device(), &allocInfo, OUT commandBuffers.Data()) == VK_SUCCESS,
            "Failed to allocate command buffer");
    }

    void Renderer::RecordCommandBuffer(int imageIndex)
    {
        static int frame = 0;
        frame = (frame + 1) % 200;
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        SNEK_ASSERT(vkBeginCommandBuffer(OUT commandBuffers[imageIndex], &beginInfo) == VK_SUCCESS,
            "Failed to begin recording command buffer");
        
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = swapChain.GetRenderPass();
        renderPassInfo.framebuffer = swapChain.GetFrameBuffer(imageIndex);
        
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChain.GetSwapChainExtent();

        u32 clearValueCount = 2;
        VkClearValue clearValues[clearValueCount];

        // CAN CHANGE BACKGROUND COLOR HERE 
        // TODO: change this to take in a dynamic color
        clearValues[0].color = {0.01f, 0.01f, 0.01f, 1.0f};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = clearValueCount;
        renderPassInfo.pClearValues = clearValues;
        
        vkCmdBeginRenderPass(OUT commandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapChain.GetSwapChainExtent().width);
        viewport.height = static_cast<float>(swapChain.GetSwapChainExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor {{0,0}, swapChain.GetSwapChainExtent()};

        vkCmdSetViewport(commandBuffers[imageIndex], 0, 1, &viewport);
        vkCmdSetScissor(commandBuffers[imageIndex], 0, 1, &scissor);

        graphicsPipeline.Bind(commandBuffers[imageIndex]);

        size_t modelIdx = 0;
        for (auto& model : models)
        {
            model->Bind(commandBuffers[imageIndex]);

            Model::ModelPushConstantData data{};
            data.offset = {-0.5f + frame * 0.01f, -0.4f + modelIdx * 0.25f};
            data.color = {0.0f, 0.0f, 0.2f + 0.2f * modelIdx};

            vkCmdPushConstants(
                commandBuffers[imageIndex],
                pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(Model::ModelPushConstantData),
                &data
            );

            model->Draw(commandBuffers[imageIndex]);
            modelIdx++;
        }
        
        vkCmdEndRenderPass(OUT commandBuffers[imageIndex]);

        SNEK_ASSERT(vkEndCommandBuffer(OUT commandBuffers[imageIndex]) == VK_SUCCESS,
            "Failed to record command buffer!");
    }

    void Renderer::RecreateSwapChain()
    {
        auto extent = window.GetExtent();
        while(extent.width == 0 || extent.height == 0)
        {
            extent = window.GetExtent();
            window.WaitEvents();
        }

        // Clear our graphics pipeline before swapchain re-creation
        graphicsPipeline.ClearPipeline();

        // Re-create swapchain
        swapChain.RecreateSwapchain();

        if (swapChain.GetImageCount() != commandBuffers.Size())
        {
            FreeCommandBuffers();
            CreateCommandBuffers();
        }

        // Re-create the pipeline once the swapchain renderpass 
        // becomes available again.

        // NOTE: We could possibly avoid this if we check for render pass compatibility. 
        // If the new renderpass is compatible with the old, then we can actually keep the 
        // the same graphics pipeline.
        graphicsPipeline.RecreatePipeline(
            "bin/shaders/simpleShader.vert.spv", 
            "bin/shaders/simpleShader.frag.spv", 
            CreateDefaultPipelineConfig()
        );
    }

    void Renderer::FreeCommandBuffers()
    {
        vkFreeCommandBuffers(
            device.Device(), 
            device.GetCommandPool(), 
            swapChain.GetImageCount(), 
            commandBuffers.Data());
    }

    PipelineConfigInfo Renderer::CreateDefaultPipelineConfig()
    {
        auto pipelineConfig = Pipeline::DefaultPipelineConfig();
        pipelineConfig.renderPass = swapChain.GetRenderPass();
        pipelineConfig.pipelineLayout = pipelineLayout;

        return pipelineConfig;
    }

    void Renderer::CreatePipelineLayout()
    {
        VkPushConstantRange range{};
        range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        range.offset = 0; 
        range.size = sizeof(Model::ModelPushConstantData);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0; 
        pipelineLayoutInfo.pSetLayouts = nullptr;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &range;

        SNEK_ASSERT(vkCreatePipelineLayout(device.Device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS, 
            "Failed to create pipeline layout!");
    }

    Pipeline Renderer::CreateGraphicsPipeline()
    {
        CreatePipelineLayout();

        SNEK_ASSERT(pipelineLayout != nullptr, "Cannot create pipeline without a valid layout!");

        return SnekVk::Pipeline(
            device, 
            "bin/shaders/simpleShader.vert.spv", 
            "bin/shaders/simpleShader.frag.spv", 
            CreateDefaultPipelineConfig()
        );
    }

    void Renderer::DrawFrame()
    {
        u32 imageIndex;
        auto result = swapChain.AcquireNextImage(&imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) 
        {
            RecreateSwapChain();
            return;
        }

        SNEK_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR, 
            "Failed to acquire swapchain image!");
        
        RecordCommandBuffer(imageIndex);
        result = swapChain.SubmitCommandBuffers(&commandBuffers[imageIndex], &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
            window.WasResized())
        {
            window.ResetWindowResized();
            RecreateSwapChain();
            return;
        }

        SNEK_ASSERT(result == VK_SUCCESS, "Failed to submit command buffer for drawing!");
    }
}