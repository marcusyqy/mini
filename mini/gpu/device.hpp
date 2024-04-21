#pragma once
#include <vulkan/vulkan.h>

/// @TODO: remove dependency on glfw.

#include "core/memory.hpp"
#include "defs.hpp"

// forward declare
struct GLFWwindow;

struct Device {
  VkInstance instance              = VK_NULL_HANDLE; // for convenience
  VkAllocationCallbacks* allocator = nullptr; // for convenience

  VkDevice logical                 = VK_NULL_HANDLE;
  VkPhysicalDevice physical        = VK_NULL_HANDLE;
  VkQueue queue                    = VK_NULL_HANDLE;
  u32 queue_family                 = (u32)-1;
};

VkInstance init_gpu_instance(Temp_Linear_Allocator arena);
void cleanup_gpu_instance();

Device create_device(Temp_Linear_Allocator arena);
void destroy_device(Device device);

/// create surface for now.
VkSurfaceKHR platform_create_vk_surface(GLFWwindow* window);
void platform_destroy_vk_surface(VkSurfaceKHR surface);
