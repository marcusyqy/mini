#pragma once
#include "defs.hpp"
#include "mem.hpp"
#include <vulkan/vulkan.h>
#include "imgui.h"

struct GLFWwindow;

namespace draw {

using Vk_Arena = Linear_Allocator<mega_bytes(1)>;
void setup_vulkan(const char** extensions, u32 count);
void cleanup_vulkan();

struct Window {
  GLFWwindow* window       = nullptr;
  VkSurfaceKHR surface     = VK_NULL_HANDLE;
};

// this will be used as the main window.
Window create_surface(GLFWwindow* window);
void destroy_surface(Window& window);

void new_frame(const Window& window);
void set_clear_color(const Window& window, float x, float y, float z, float w);
void render_frame(const Window& window, ImDrawData* draw_data);
void present_frame(const Window& window);

} // namespace draw
