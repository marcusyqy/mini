#pragma once
#include <vulkan/vulkan.h>

/// @TODO: remove dependency on glfw.

#include "core/memory.hpp"
#include "defs.hpp"

// forward declare
struct GLFWwindow;

struct Device {
  VkDevice logical = VK_NULL_HANDLE;
  VkPhysicalDevice physical = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  u32 queue_family = (u32)-1;
};

VkInstance init_gpu(Linear_Allocator& arena);
void cleanup_gpu();

Device create_device(Linear_Allocator& arena);
void destroy_device(Device device);

/// create surface for now.
VkSurfaceKHR platform_create_vk_surface(GLFWwindow* window);