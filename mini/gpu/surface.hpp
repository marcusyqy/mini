#pragma once
#include "core/memory.hpp"
#include "defs.hpp"
#include "sync.hpp"
#include <vulkan/vulkan.h>

struct GLFWwindow;
struct Device;

struct Surface {
  static constexpr auto MAX_IMAGES = 3;

  VkSurfaceKHR surface     = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;

  u32 frame_idx = 0;

  // this should be enough
  s32 width  = -1;
  s32 height = -1;

  VkSurfaceFormatKHR format = {};

  // FRAME STUFF
  VkImage images[MAX_IMAGES];
  VkImageView image_views[MAX_IMAGES];
  VkSemaphore image_avail[MAX_IMAGES];
  VkSemaphore render_done[MAX_IMAGES];

  s8 num_images = 0;
};

Surface create_surface(Temp_Linear_Allocator arena, Device& device, GLFWwindow* window, s16 width, s16 height);
void destroy_surface(Device& device, Surface& surface);
