#include "Renderer.h"

namespace SnekVk
{
    VkCommandBuffer* Renderer::commandBuffers;

    Renderer::Renderer(SwapChain* swapChain) 
        : swapChain{swapChain} 
    {
        commandBuffers = new VkCommandBuffer[swapChain->GetImageCount()];
    }
    
    Renderer::~Renderer() {}

    void Renderer::CreateCommandBuffers(VulkanDevice& device, Pipeline& pipeline)
    {
        u32 size = swapChain->GetImageCount();

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = device.GetCommandPool();
        allocInfo.commandBufferCount = size;

        SNEK_ASSERT(vkAllocateCommandBuffers(device.Device(), &allocInfo, commandBuffers) == VK_SUCCESS,
            "Failed to allocate command buffer");

        for (u32 i = 0; i < size; i++)
        {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            SNEK_ASSERT(vkBeginCommandBuffer(commandBuffers[i], &beginInfo) == VK_SUCCESS,
                "Failed to begin recording command buffer");
            
            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = swapChain->GetRenderPass();
            renderPassInfo.framebuffer = swapChain -> GetFrameBuffer(i);
            
            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = swapChain->GetSwapChainExtent();

            u32 clearValueCount = 2;
            VkClearValue clearValues[clearValueCount];
            clearValues[0].color = {0.1f, 0.1f, 0.1f, 1.0f};
            clearValues[1].depthStencil = {1.0f, 0};

            renderPassInfo.clearValueCount = clearValueCount;
            renderPassInfo.pClearValues = clearValues;
            
            vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            pipeline.Bind(commandBuffers[i]);
            vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

            vkCmdEndRenderPass(commandBuffers[i]);

            SNEK_ASSERT(vkEndCommandBuffer(commandBuffers[i]) == VK_SUCCESS,
                "Failed to record command buffer!");
        }
    }

    void Renderer::DrawFrame()
    {
        u32 imageIndex;
        auto result = swapChain->AcquireNextImage(&imageIndex);

        SNEK_ASSERT(result == VK_SUCCESS && result != VK_SUBOPTIMAL_KHR, 
            "Failed to acquire swapchain image!");
        
        result = swapChain->SubmitCommandBuffers(&commandBuffers[imageIndex], &imageIndex);

        SNEK_ASSERT(result == VK_SUCCESS, "Failed to submit command buffer for drawing!");
    }
}