#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "imgui.h"
#include "imgui_impl_glfw.h"

#include "defs.hpp"

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN

#include "constant/roboto.font"
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include "draw.hpp"
#include "log.hpp"

static void glfw_error_callback(int error, const char* description) {
  log_error("GLFW Error %d: %s", error, description);
}

int main(int, char**) {
  log_info("Hello world from %s!!", "Mini Engine");

  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit()) return 1;

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  constexpr auto window_name = "Place holder window name";
  GLFWwindow* window         = glfwCreateWindow(1280, 720, window_name, nullptr, nullptr);
  if (!glfwVulkanSupported()) {
    log_error("GLFW: Vulkan not supported");
    return 1;
  }

  u32 extension_count     = {};
  const char** extensions = glfwGetRequiredInstanceExtensions(&extension_count);
  auto vk_vars = draw::setup_vulkan(extensions, extension_count);


  return 0;
}
