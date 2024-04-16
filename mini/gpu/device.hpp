#pragma once
#include <vulkan/vulkan.h>

/// @TODO: remove dependency on glfw.

#include <GLFW/glfw3.h>
#include "core/memory.hpp"

struct Device {
  VkDevice logical;
  VkPhysicalDevice physical;
};


struct Device_Properties {

};

VkInstance init_gpu(Linear_Allocator& arena);
void cleanup_gpu();

Device create_device(Device_Properties properties = {});

/// create surface for now.
VkSurfaceKHR platform_create_surface(GLFWwindow* window);
