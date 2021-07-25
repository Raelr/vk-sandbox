#include "SwapChainSupportDetails.h"

namespace SnekVk
{
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR& surface) 
    {
        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

        if (formatCount != 0) 
        {
            details.formats = new VkSurfaceFormatKHR[formatCount];
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats);
            
            details.availableFormatCount = formatCount;
            details.hasFormats = true;
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

        if (presentModeCount != 0) 
        {
            details.presentModes = new VkPresentModeKHR[presentModeCount];
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                device,
                surface,
                &presentModeCount,
                details.presentModes);

            details.availablePresentModeCount = presentModeCount;
            details.hasPresentModes = true;
        }
        return details;
    }

    void DestroySwapChainSupportDetails(SwapChainSupportDetails& details)
    {
        delete [] details.formats;
        delete [] details.presentModes;
    }
}