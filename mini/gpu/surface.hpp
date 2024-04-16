#pragma once 
#include <vulkan/vulkan.h>

struct Surface {
  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;
};
