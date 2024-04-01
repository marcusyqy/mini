#pragma once
#include "defs.hpp"
#include "mem.hpp"
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

namespace draw {

using Vk_Stack_Allocator = Stack_Allocator<mega_bytes(1)>;
void setup_vulkan(const char** extensions, u32 count);
void cleanup_vulkan();

struct Window {
  GLFWwindow* window = nullptr;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
};

// this will be used as the main window.
Window create_surface(GLFWwindow* window);

} // namespace draw
