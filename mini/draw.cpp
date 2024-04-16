
// for imgui
// @TODO: remove this. (just use this for now).
#include "gpu/device.hpp"
#include "gpu/surface.hpp"
#include "imgui_impl_vulkan.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// #define GLFW_EXPOSE_NATIVE_WIN32
// #include <GLFW/glfw3native.h>

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

// #if defined(_WIN32)
// #include <windows.h>
// #undef max
// #include <vulkan/vulkan_win32.h>
// #endif

#include "draw.hpp"
#include "log.hpp"

// std libs
#include "core/memory.hpp"
#include <cassert>
#include <cstdio>

#define VK_ENABLE_VALIDATION 1

namespace draw {

static void vk_check(VkResult err) {
  if (err == VK_SUCCESS) return;
  log_error("[vulkan] Error: VkResult = %d", string_VkResult(err));
  assert(err < 0);
}

// globals
static VkAllocationCallbacks* allocator_callback = nullptr;
static VkInstance instance                       = VK_NULL_HANDLE;
static Device device                             = {};
static VkPipelineCache pipeline_cache            = VK_NULL_HANDLE;
static VkDescriptorPool descriptor_pool          = VK_NULL_HANDLE;
static Linear_Allocator arena                    = { mega_bytes(1) };
constexpr static int swapchain_min_image_count   = 2;

namespace vk_state {
static bool swapchain_rebuild = false;
}

void setup_vulkan() {
  instance = init_gpu(arena);
  device   = create_device(arena);

  // Create Descriptor Pool
  // The example only requires a single combined image sampler descriptor for the font image and only uses one
  // descriptor set (for that) If you wish to load e.g. additional textures you may need to alter pools sizes.
  {
    VkDescriptorPoolSize pool_sizes[] = {
      { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags                      = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets                    = 1;
    pool_info.poolSizeCount              = 1;
    pool_info.pPoolSizes                 = pool_sizes;
    vk_check(vkCreateDescriptorPool(device.logical, &pool_info, allocator_callback, &descriptor_pool));
  }
}

void cleanup_vulkan() {
  vkDestroyDescriptorPool(device.logical, descriptor_pool, allocator_callback);
  destroy_device(device);
  cleanup_gpu();
}

namespace to_remove {
ImGui_ImplVulkanH_Window main_window_imgui_impl = {};
}

// I think you are only able to create one here because we need to manage people's expectations.
Window create_surface(GLFWwindow* window) {
  // Create Window Surface
  Surface s            = ::create_surface(arena, device, window);
  VkSurfaceKHR surface = s.surface;

  // Create Framebuffers
  ImGui_ImplVulkanH_Window* wd = &to_remove::main_window_imgui_impl;
  wd->Surface                  = surface;

  // Select Surface Format
  const VkFormat requestSurfaceImageFormat[]     = { VK_FORMAT_B8G8R8A8_UNORM,
                                                     VK_FORMAT_R8G8B8A8_UNORM,
                                                     VK_FORMAT_B8G8R8_UNORM,
                                                     VK_FORMAT_R8G8B8_UNORM };
  const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
  wd->SurfaceFormat                              = ImGui_ImplVulkanH_SelectSurfaceFormat(
      device.physical,
      wd->Surface,
      requestSurfaceImageFormat,
      (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat),
      requestSurfaceColorSpace);

  log_debug("made it here maybe");
  // Select Present Mode
#ifdef APP_USE_UNLIMITED_FRAME_RATE
  VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR,
                                       VK_PRESENT_MODE_IMMEDIATE_KHR,
                                       VK_PRESENT_MODE_FIFO_KHR };
#else
  VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
#endif
  wd->PresentMode =
      ImGui_ImplVulkanH_SelectPresentMode(device.physical, wd->Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));
  // printf("[vulkan] Selected PresentMode = %d\n", wd->PresentMode);

  // Create SwapChain, RenderPass, Framebuffer, etc.
  IM_ASSERT(swapchain_min_image_count >= 2);
  ImGui_ImplVulkanH_CreateOrResizeWindow(
      instance,
      device.physical,
      device.logical,
      wd,
      device.queue_family,
      allocator_callback,
      s.width,
      s.height,
      swapchain_min_image_count);

  // Setup Platform/Renderer backends
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance                  = instance;
  init_info.PhysicalDevice            = device.physical;
  init_info.Device                    = device.logical;
  init_info.QueueFamily               = device.queue_family;
  init_info.Queue                     = device.queue;
  init_info.PipelineCache             = pipeline_cache;
  init_info.DescriptorPool            = descriptor_pool;
  init_info.RenderPass                = wd->RenderPass;
  init_info.Subpass                   = 0;
  init_info.MinImageCount             = swapchain_min_image_count;
  init_info.ImageCount                = wd->ImageCount;
  init_info.MSAASamples               = VK_SAMPLE_COUNT_1_BIT;
  init_info.Allocator                 = allocator_callback;
  init_info.CheckVkResultFn           = vk_check;
  ImGui_ImplVulkan_Init(&init_info);

  return { window, surface };
}

void destroy_surface(Window& window) {
  assert(to_remove::main_window_imgui_impl.Surface == window.surface);

  vk_check(vkDeviceWaitIdle(device.logical));

  // just to do it correctly.
  Surface s;
  s.surface = window.surface;
  ::destroy_surface(device, s);

  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplVulkanH_DestroyWindow(instance, device.logical, &to_remove::main_window_imgui_impl, allocator_callback);
}

void new_frame(const Window& window) {
  assert(to_remove::main_window_imgui_impl.Surface == window.surface);
  if (vk_state::swapchain_rebuild) {
    int width, height;
    glfwGetFramebufferSize(window.window, &width, &height);
    if (width > 0 && height > 0) {
      ImGui_ImplVulkan_SetMinImageCount(swapchain_min_image_count);
      ImGui_ImplVulkanH_CreateOrResizeWindow(
          instance,
          device.physical,
          device.logical,
          &to_remove::main_window_imgui_impl,
          device.queue_family,
          allocator_callback,
          width,
          height,
          swapchain_min_image_count);
      to_remove::main_window_imgui_impl.FrameIndex = 0;
      vk_state::swapchain_rebuild                  = false;
    }
  }
  // Start the Dear ImGui frame
  ImGui_ImplVulkan_NewFrame();
}

void present_frame(const Window& window) {
  assert(to_remove::main_window_imgui_impl.Surface == window.surface);
  if (vk_state::swapchain_rebuild) return;

  ImGui_ImplVulkanH_Window* wd          = &to_remove::main_window_imgui_impl;
  VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
  VkPresentInfoKHR info                 = {};
  info.sType                            = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  info.waitSemaphoreCount               = 1;
  info.pWaitSemaphores                  = &render_complete_semaphore;
  info.swapchainCount                   = 1;
  info.pSwapchains                      = &wd->Swapchain;
  info.pImageIndices                    = &wd->FrameIndex;
  VkResult err                          = vkQueuePresentKHR(device.queue, &info);
  if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
    vk_state::swapchain_rebuild = true;
    return;
  }
  vk_check(err);
  wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount; // Now we can use the next set of semaphores
}

void set_clear_color(const Window& window, float x, float y, float z, float w) {
  assert(to_remove::main_window_imgui_impl.Surface == window.surface);
  ImGui_ImplVulkanH_Window* wd    = &to_remove::main_window_imgui_impl;
  wd->ClearValue.color.float32[0] = x;
  wd->ClearValue.color.float32[1] = y;
  wd->ClearValue.color.float32[2] = z;
  wd->ClearValue.color.float32[3] = w;
}

// TO_REMOVE
void render_frame(const Window& window, ImDrawData* draw_data) {
  assert(to_remove::main_window_imgui_impl.Surface == window.surface);
  ImGui_ImplVulkanH_Window* wd = &to_remove::main_window_imgui_impl;
  VkResult err;

  VkSemaphore image_acquired_semaphore  = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
  VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;

  err = vkAcquireNextImageKHR(
      device.logical,
      wd->Swapchain,
      UINT64_MAX,
      image_acquired_semaphore,
      VK_NULL_HANDLE,
      &wd->FrameIndex);

  if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
    vk_state::swapchain_rebuild = true;
    return;
  }
  vk_check(err);

  ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
  {
    err = vkWaitForFences(
        device.logical,
        1,
        &fd->Fence,
        VK_TRUE,
        UINT64_MAX); // wait indefinitely instead of periodically checking
    vk_check(err);

    err = vkResetFences(device.logical, 1, &fd->Fence);
    vk_check(err);
  }
  {
    err = vkResetCommandPool(device.logical, fd->CommandPool, 0);
    vk_check(err);
    VkCommandBufferBeginInfo info = {};
    info.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
    vk_check(err);
  }
  {
    VkRenderPassBeginInfo info    = {};
    info.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass               = wd->RenderPass;
    info.framebuffer              = fd->Framebuffer;
    info.renderArea.extent.width  = wd->Width;
    info.renderArea.extent.height = wd->Height;
    info.clearValueCount          = 1;
    info.pClearValues             = &wd->ClearValue;
    vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
  }

  // Record dear imgui primitives into command buffer
  ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

  // Submit command buffer
  vkCmdEndRenderPass(fd->CommandBuffer);
  {
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo info               = {};
    info.sType                      = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.waitSemaphoreCount         = 1;
    info.pWaitSemaphores            = &image_acquired_semaphore;
    info.pWaitDstStageMask          = &wait_stage;
    info.commandBufferCount         = 1;
    info.pCommandBuffers            = &fd->CommandBuffer;
    info.signalSemaphoreCount       = 1;
    info.pSignalSemaphores          = &render_complete_semaphore;

    err = vkEndCommandBuffer(fd->CommandBuffer);
    vk_check(err);
    err = vkQueueSubmit(device.queue, 1, &info, fd->Fence);
    vk_check(err);
  }
}

} // namespace draw
