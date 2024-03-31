#pragma once
#include "defs.hpp"
#include "mem.hpp"
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

namespace draw {

using Vk_Stack_Allocator = Stack_Allocator<mega_bytes(1)>;
void setup_vulkan(const char** extensions, u32 count);
void cleanup_vulkan();

struct Window {
  GLFWwindow* window;
  VkSurfaceKHR surface;
};

// this will be used as the main window.
Window create_surface(GLFWwindow* window);

} // namespace draw
