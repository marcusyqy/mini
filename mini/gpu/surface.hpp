#pragma once
#include "defs.hpp"
#include <vulkan/vulkan.h>
#include "core/memory.hpp"

struct GLFWwindow;
struct Device;

struct Surface {
  static constexpr auto MAX_IMAGES = 3;

  VkSurfaceKHR surface     = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;

  VkSurfaceFormatKHR format = {};

  // FRAME STUFF
  VkImageView image_views[MAX_IMAGES];
  VkSemaphore image_avail[MAX_IMAGES];
  VkSemaphore render_done[MAX_IMAGES];

  // this should be enough
  s16 width  = -1;
  s16 height = -1;

  s8 num_images = 0;
  s8 frame_idx  = 0;
};

Surface create_surface(Temp_Linear_Allocator arena, Device& device, GLFWwindow* window, s16 width, s16 height);
void destroy_surface(Device& device, Surface& surface);
