#pragma once

#include "Renderer.h"
#include "../Window/Window.h"
#include "Utils/DebugUtilsMessenger.h"
#include "Utils/Extensions.h"
#include "Utils/QueueFamilyIndices.h"
#include "Utils/PhysicalDevice.h"
#include "Utils/SwapChainSupportDetails.h"

// std lib headers
#include <string>
#include <vector>
#include <array>

namespace SnekVk {

class VulkanDevice {
	public:

#ifdef DEBUG
	const bool enableValidationLayers = true;
#else
    const bool enableValidationLayers = false;
#endif

    VulkanDevice(SnekVk::Window &window);
    ~VulkanDevice();

    // Not copyable or movable
    VulkanDevice(const VulkanDevice &) = delete;
    void operator=(const VulkanDevice &) = delete;
    VulkanDevice(VulkanDevice &&) = delete;
    VulkanDevice &operator=(VulkanDevice &&) = delete;

    VkCommandPool GetCommandPool() { return commandPool; }
    VkDevice Device() { return device; }
    VkSurfaceKHR Surface() { return surface; }
    VkQueue GraphicsQueue() { return graphicsQueue; }
    VkQueue PresentQueue() { return presentQueue; }

    SwapChainSupportDetails getSwapChainSupport() { return SnekVk::QuerySwapChainSupport(physicalDevice, surface); }
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    QueueFamilyIndices findPhysicalQueueFamilies() { return SnekVk::FindQueueFamilies(physicalDevice, surface); }
    VkFormat FindSupportedFormat(
        const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

	// Buffer Helper Functions
	void CreateBuffer(
		VkDeviceSize size,
		VkBufferUsageFlags usage,
		VkMemoryPropertyFlags properties,
		VkBuffer &buffer,
		VkDeviceMemory &bufferMemory);
	VkCommandBuffer BeginSingleTimeCommands();
	void EndSingleTimeCommands(VkCommandBuffer commandBuffer);
	void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	void CopyBufferToImage(
		VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount);

	void CreateImageWithInfo(
		const VkImageCreateInfo &imageInfo,
		VkMemoryPropertyFlags properties,
		VkImage &image,
		VkDeviceMemory &imageMemory);

	VkPhysicalDeviceProperties properties;

	static void DestroyVulkanDevice(VulkanDevice& device);

 	private:
	void CreateInstance();
	void SetupDebugMessenger();
	void CreateSurface();
	void PickPhysicalDevice();
	void CreateLogicalDevice();
	void CreateCommandPool();

	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	Window &window;
	VkCommandPool commandPool;

	VkDevice device;
	VkSurfaceKHR surface;
	VkQueue graphicsQueue;
	VkQueue presentQueue;

	const std::array<const char *, 1> validationLayers = { "VK_LAYER_KHRONOS_validation" };
	const std::array<const char *, 1> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
};

}  // namespace lve