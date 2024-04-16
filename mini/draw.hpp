#pragma once
#include "core/memory.hpp"
#include "defs.hpp"
#include "imgui.h"
#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace draw {

// @TODO we can remove extensions and count when we totally bake the extensions inside
void setup_vulkan();
void cleanup_vulkan();

struct Window {
  GLFWwindow* window   = nullptr;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
};

// this will be used as the main window.
Window create_surface(GLFWwindow* window);
void destroy_surface(Window& window);

void new_frame(const Window& window);
void set_clear_color(const Window& window, float x, float y, float z, float w);
void render_frame(const Window& window, ImDrawData* draw_data);
void present_frame(const Window& window);

/// SCOPE OUT DRAW API HERE.

} // namespace draw
