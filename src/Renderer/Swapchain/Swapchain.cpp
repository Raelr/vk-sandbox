#include "Swapchain.h"

#include <array>

namespace SnekVk
{
    SwapChain::SwapChain(VulkanDevice& device, VkExtent2D windowExtent) 
        : device{device}, windowExtent{windowExtent}
    {
        CreateSwapChain();
        CreateImageViews();
        CreateRenderPass();
        CreateDepthResources();
        CreateFrameBuffers();
        CreateSyncObjects();
    }

    SwapChain::~SwapChain() 
    {
        u32 imageCount = GetImageCount(); 
        for (u32 i = 0; i < imageCount; i++)
        {
            vkDestroyImageView(device.Device(), swapChainImageViews[i], nullptr);
        }

        delete [] swapChainImageViews;

        if (GetSwapChain() != nullptr)
        {
            vkDestroySwapchainKHR(device.Device(), GetSwapChain(), nullptr);
            swapChain = nullptr;
        }

        for (size_t i = 0; i < imageCount; i++)
        {
            vkDestroyImageView(device.Device(), depthImageViews[i], nullptr);
            vkDestroyImage(device.Device(), depthImages[i], nullptr);
            vkFreeMemory(device.Device(), depthImageMemorys[i], nullptr);
        }

        delete [] depthImageViews;
        delete [] depthImages;
        delete [] depthImageMemorys;

        for (size_t i = 0; i < imageCount; i++)
        {
            vkDestroyFramebuffer(device.Device(), swapChainFrameBuffers[i], nullptr);
        }

        delete [] swapChainFrameBuffers;
        
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroySemaphore(device.Device(), renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device.Device(), imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(device.Device(), inFlightFences[i], nullptr);
        }

        delete [] imagesInFlight;
    }

    void SwapChain::CreateSwapChain()
    {
        // Get our swapchain details
        SwapChainSupportDetails::SwapChainSupportDetails details = device.GetSwapChainSupport();

        // Get our supported color format
        VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(details.formats, details.availableFormatCount);

        // Choose our presentation mode (the form of image buffering)
        VkPresentModeKHR presentMode = ChoosePresentMode(details.presentModes, details.availablePresentModeCount);

        // The size of our images. 
        VkExtent2D extent = ChooseSwapExtent(details.capabilities);

        std::cout << "Extent: " << extent.width << "x" << extent.height << std::endl;

        // Get the image count we can support 
        u32 imageCount = details.capabilities.minImageCount + 1;

        // Make sure we aren't exceeding the GPU's image count maximums
        if (details.capabilities.maxImageCount > 0 && imageCount > details.capabilities.maxImageCount)
        {
            imageCount = details.capabilities.maxImageCount;
        }   
        std::cout << "Image Count: " << imageCount << std::endl;

        // Now we populate the base swapchain creation struct
        VkSwapchainCreateInfoKHR createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = device.Surface();
        
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        
        // Get our image queue information for rendering
        QueueFamilyIndices::QueueFamilyIndices indices = device.FindPhysicalQueueFamilies();
        u32 queueFamilyIndices[] = {indices.graphicsFamily, indices.presentFamily};

        // Sometimes the graphics and presentation queues are the same. We want to check for this 
        // eventuality. 
        if (indices.graphicsFamily != indices.presentFamily)
        {
            // We specify that there are two distinct queues that need to be used
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else 
        {
            // Indicates that there is only a single queue to be used
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        // Indicates any pre-transforms that need to occur to images (default is none)
        createInfo.preTransform = details.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;

        // specifies if there's an old swapchain for it to copy data from
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        SNEK_ASSERT(vkCreateSwapchainKHR(device.Device(), &createInfo, nullptr, OUT &swapChain) == VK_SUCCESS,
                "Failed to create Swapchain!");

        // Once the swapchain has been created, get the number of images supported. 
        vkGetSwapchainImagesKHR(device.Device(), swapChain, OUT &imageCount, nullptr);
        swapChainImages = new VkImage[imageCount];
        // Initialise the images
        vkGetSwapchainImagesKHR(device.Device(), swapChain, &imageCount, OUT swapChainImages);

        // Finally, set the corresponding values in the Swapchain.
        this->imageCount = imageCount;
        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;

        SwapChainSupportDetails::DestroySwapChainSupportDetails(details);
    }

    void SwapChain::CreateImageViews()
    {
        swapChainImageViews = new VkImageView[imageCount];

        // Create our image views for each image supported:
        for (size_t i = 0; i < imageCount; i++)
        {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapChainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapChainImageFormat;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            SNEK_ASSERT(vkCreateImageView(device.Device(), &createInfo, nullptr, OUT &swapChainImageViews[i]) == VK_SUCCESS,
                    "Failed to create texture image view!");
        }
    }

    void SwapChain::CreateRenderPass()
    {
        VkAttachmentDescription depthAttachment = Attachments::CreateDepthAttachment(FindDepthFormat());

        VkAttachmentReference depthAttachmentRef = Attachments::CreateDepthStencilAttachmentReference(1);

        // Specify our color attachments and how we want them to be displayed
        VkAttachmentDescription colorAttachment = Attachments::CreateColorAttachment(GetSwapChainImageFormat());

        VkAttachmentReference colorAttachmentRef = Attachments::CreateColorAttachmentReference(0);

        VkSubpassDescription subpass = Attachments::CreateGraphicsSubpass(1,
                                                                          &colorAttachmentRef,
                                                                          &depthAttachmentRef);

        VkSubpassDependency dependency = Attachments::CreateDependency(VK_SUBPASS_EXTERNAL,
                                                                       0,
                                                                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                                                                       | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                                                                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                                                                       | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                                                                       0,
                                                                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                                                                       | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

        // Specify the number of attachments being used by this renderpass
        u32 attachmentCount = 2;
        VkAttachmentDescription attachments[] = {colorAttachment, depthAttachment};

        renderPass.Build(&device,
                         attachments,
                         attachmentCount,
                         &subpass,
                         1,
                         &dependency,
                         1);
    }

    void SwapChain::CreateDepthResources()
    {
        // Start by getting our depth information
        VkFormat format = FindDepthFormat();
        VkExtent2D swapChainExtent = GetSwapChainExtent();

        // Initialise our depth image information. 
        depthImages = new VkImage[imageCount];
        depthImageMemorys = new VkDeviceMemory[imageCount];
        depthImageViews = new VkImageView[imageCount];

        for (size_t i = 0; i < imageCount; i++)
        {
            // Specify the type of image
            VkImageCreateInfo imageInfo{};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.extent.width = swapChainExtent.width;
            imageInfo.extent.height = swapChainExtent.height;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = 1;
            imageInfo.format = format;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageInfo.flags = 0;

            // Create the depth images using the device
            device.CreateImageWithInfo(
                imageInfo, 
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
                OUT depthImages[i], 
                OUT depthImageMemorys[i]);

            // Set up the view image creation struct
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = depthImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = format;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            SNEK_ASSERT(vkCreateImageView(device.Device(), &viewInfo, nullptr, OUT &depthImageViews[i]) == VK_SUCCESS,
                "Failed to create texture image view!");
        }
    }

    void SwapChain::CreateFrameBuffers()
    {
        // Initialise the framebuffers storage
        swapChainFrameBuffers = new VkFramebuffer[imageCount];

        // We need a separate frame buffer for each image that we want to draw
        for(size_t i = 0; i < imageCount; i++)
        {
            // We have two sets of image views we need to render images with
            u32 attachmentCount = 2;

            VkImageView attachments[] {swapChainImageViews[i], depthImageViews[i]};

            // Get our extents
            VkExtent2D swapChainExtent = GetSwapChainExtent();

            // Populate the creation struct
            VkFramebufferCreateInfo frameBufferInfo{};
            frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            frameBufferInfo.renderPass = renderPass.GetRenderPass();
            frameBufferInfo.attachmentCount = attachmentCount;
            frameBufferInfo.pAttachments = attachments;
            frameBufferInfo.width = swapChainExtent.width;
            frameBufferInfo.height = swapChainExtent.height;
            frameBufferInfo.layers = 1;

            SNEK_ASSERT(vkCreateFramebuffer(device.Device(), &frameBufferInfo, nullptr, OUT &swapChainFrameBuffers[i]) == VK_SUCCESS,
                "Failed to create framebuffer");
        }
    }

    void SwapChain::CreateSyncObjects()
    {
        // A semaphor for holding images that are available for use. We create one per frame in flight.
        imageAvailableSemaphores = new VkSemaphore[MAX_FRAMES_IN_FLIGHT];
        // A semaphor for holding images that are finished rendering. We create one per frame in flight.
        renderFinishedSemaphores = new VkSemaphore[MAX_FRAMES_IN_FLIGHT];
        
        // A set of fences for holding our max number of images that can be swapped at once
        inFlightFences = new VkFence[MAX_FRAMES_IN_FLIGHT];
        imagesInFlight = new VkFence[imageCount];

        // Set all images in flight to null
        for (size_t i = 0; i < imageCount; i++) imagesInFlight[i] = VK_NULL_HANDLE;

        // Create our semaphore and fence create info
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        // Create the synchronisation objects
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            SNEK_ASSERT(
                vkCreateSemaphore(device.Device(), &semaphoreInfo, nullptr, OUT &imageAvailableSemaphores[i]) == VK_SUCCESS &&
                vkCreateSemaphore(device.Device(), &semaphoreInfo, nullptr, OUT &renderFinishedSemaphores[i]) == VK_SUCCESS &&
                vkCreateFence(device.Device(), &fenceInfo, nullptr, OUT &inFlightFences[i]) == VK_SUCCESS, 
                "Failed to create synchronization objects fora  frame!");
        }
    }

    VkResult SwapChain::AcquireNextImage(u32* imageIndex)
    {
        // Wait for the image of the current frame to become available
        vkWaitForFences(
            device.Device(), 
            1, 
            &inFlightFences[currentFrame], 
            VK_TRUE, 
            std::numeric_limits<u64>::max());

        // Once available, Add it to our available images semaphor for usage
        return vkAcquireNextImageKHR(
            device.Device(),
            swapChain, 
            std::numeric_limits<u64>::max(),
            imageAvailableSemaphores[currentFrame],
            VK_NULL_HANDLE,
            imageIndex
        ); 
    }

    VkResult SwapChain::SubmitCommandBuffers(const VkCommandBuffer* buffers, u32* imageIndex)
    {
        u32 index = *imageIndex;

        // If the image being asked for is being used, we wait for it to become available
        if (imagesInFlight[index] != VK_NULL_HANDLE)
        {
            vkWaitForFences(device.Device(), 1, &imagesInFlight[index], VK_TRUE, UINT64_MAX);
        }

        // Get the frame's image and move it to our images in flight
        imagesInFlight[index] = inFlightFences[currentFrame];

        // With our submission, we specify a semaphore to wait on to grab data from and one to
        // signal when the rendering is complete.

        VkSubmitInfo submitInfo{};
        
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        // Specify the semaphores we want to wait for (the one for our current frame)
        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};

        // Set a stage that you need to wait for. In this case we wait until the color stage of the pipeline is done (fragment stage)
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = buffers;

        // Specify some semaphores to be signalled when rendering is done
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        
        // Reset the fence of this frame
        vkResetFences(device.Device(), 1, OUT &inFlightFences[currentFrame]);

        // Submit the command buffer to the graphics queue
        SNEK_ASSERT(vkQueueSubmit(device.GraphicsQueue(), 1, &submitInfo, OUT inFlightFences[currentFrame]) == VK_SUCCESS,
            "Failed to submit draw command buffer");

        // Set up our presentation information and the semaphores to wait on
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        // Specify the swapchains we want to use (currently only using one)
        VkSwapchainKHR swapChains[] {swapChain};

        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;

        // Store the image we want to render
        presentInfo.pImageIndices = imageIndex;

        // Submit the presentation info to the present queue.
        auto result = vkQueuePresentKHR(device.PresentQueue(), &presentInfo);

        // Set the frame to the next frame
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

        // Return the result of the rendering process
        return result;
    }

    // Helpers

    VkSurfaceFormatKHR SwapChain::ChooseSwapSurfaceFormat(VkSurfaceFormatKHR* formats, size_t formatCount)
    {
        // Ideally, we want to support colors in 4 dimensional vectors (R, G, B, A) in SRGB color space.
        for (size_t i = 0; i < formatCount; i++)
        {
            VkSurfaceFormatKHR& availableFormat = formats[i];
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return availableFormat;
        }
        // If we can't find what we want then we use whatever is available on the GPU.
        return formats[0];
    }

    VkPresentModeKHR SwapChain::ChoosePresentMode(VkPresentModeKHR* presentModes, size_t presentModeCount)
    {
        for (size_t i = 0; i < presentModeCount; i++)
        {
            VkPresentModeKHR& availablePresentMode = presentModes[i];

            // Ideally, we want to support triple buffering since it has the best 
            // balance between performance and image quality
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                std::cout << "Present Mode: Mailbox" << std::endl;
                return availablePresentMode;
            }
        }

        // If triple buffering is not available then use v-sync
        std::cout << "Present Mode: V-Sync" << std::endl;
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D SwapChain::ChooseSwapExtent(VkSurfaceCapabilitiesKHR& capabilities)
    {
        // We want to make sure that the extents provided are within a reasonable range.
        if (capabilities.currentExtent.width != std::numeric_limits<u32>::max()) return capabilities.currentExtent;
        else 
        {
            // Otherwise get the smaller extent between the window and the swapchain image
            // extents and return that.
            VkExtent2D actualExtent = windowExtent;
            actualExtent.width = std::max(
                capabilities.minImageExtent.width,
                std::min(capabilities.maxImageExtent.width, actualExtent.width)
            );
            actualExtent.height = std::max(
                capabilities.minImageExtent.height,
                std::min(capabilities.maxImageExtent.height, actualExtent.height)
            );
            return actualExtent;
        }
    }

    VkFormat SwapChain::FindDepthFormat()
    {
        // The formats we want to search for:
        // 1. A 32 bit floating point
        // 2. A 32 bit floating point for storing depth information and 8 bits for stencil data.
        // 3. A 32 bit component with 8 unsigned bits for stencil data and 24 unsigned normalised bits for depth. 
        VkFormat formats[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
        return device.FindSupportedFormat(
            formats, 
            3,
            VK_IMAGE_TILING_OPTIMAL, // specify we want texels to be laid out optimally
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT); // specifies that we can use image views to specify depth information
    }
}
