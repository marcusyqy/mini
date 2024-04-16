#pragma once
#include "defs.hpp"
#include <vulkan/vulkan.h>

struct GLFWwindow;
struct Device;
struct Linear_Allocator;

struct Surface {
  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;

  // this should be enough
  s16 width;
  s16 height;

  s8 frame_idx;

  void present();
};

Surface create_surface(Linear_Allocator& arena, Device& device, GLFWwindow* window);
void destroy_surface(Device& device, Surface& surface);
