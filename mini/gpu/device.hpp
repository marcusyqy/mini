#pragma once
#include <vulkan/vulkan.h>

/// @TODO: remove dependency on glfw.

#include <GLFW/glfw3.h>
#include "core/memory.hpp"
#include "defs.hpp"

struct Device {
  VkDevice logical = VK_NULL_HANDLE;
  VkPhysicalDevice physical = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  u32 queue_family = (u32)-1;
};


struct Device_Properties {

};

VkInstance init_gpu(Linear_Allocator& arena);
void cleanup_gpu();

Device create_device(Linear_Allocator& arena, Device_Properties properties = {});
void free_device(Device device);

/// create surface for now.
VkSurfaceKHR platform_create_surface(GLFWwindow* window);
