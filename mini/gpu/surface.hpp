#pragma once 
#include <vulkan/vulkan.h>
#include "defs.hpp"

struct GLFWwindow;
struct Device;

struct Surface {
  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;
  s32 width;
  s32 height;
};



Surface create_surface(Device& device, GLFWwindow* window);
void destroy_surface(Device& device, Surface& surface);