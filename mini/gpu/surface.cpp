#include "surface.hpp"
#include "device.hpp"
#include <GLFW/glfw3.h>

Surface create_surface(Device& device, GLFWwindow* window) {
  Surface surface;
  surface.surface = platform_create_vk_surface(window);

  // Create Framebuffers
  glfwGetFramebufferSize(window, &surface.width, &surface.height);

  VkBool32 res = VK_FALSE;
  // I think we have to call this or we get a validation error.
  vkGetPhysicalDeviceSurfaceSupportKHR(device.physical, device.queue_family, surface.surface, &res);
  assert(res == VK_TRUE);


  /// @TODO: swapchain not initialized
  return surface;
}

void destroy_surface(Device& device, Surface& surface) {
  static_cast<void>(device);
  static_cast<void>(surface);
  // nothing to do here.
}