
// for imgui
// @TODO: remove this. (just use this for now).
#include "gpu/device.hpp"
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

namespace vk_constants {
constexpr auto api_version             = VK_API_VERSION_1_3;
constexpr const char* validation_layer = "VK_LAYER_KHRONOS_validation";

// we need to compare with this.
constexpr const char* required_extensions[] = {
  "VK_KHR_surface",
#ifdef WIN32
  "VK_KHR_win32_surface",
#endif
#if VK_ENABLE_VALIDATION
  VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
  VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
};
} // namespace vk_constants

static void vk_check(VkResult err) {
  if (err == VK_SUCCESS) return;
  log_error("[vulkan] Error: VkResult = %d", string_VkResult(err));
  assert(err < 0);
}

VkBool32 vk_get_physical_device_present_support(VkInstance instance, VkPhysicalDevice physical_device, u32 index) {
  return glfwGetPhysicalDevicePresentationSupport(instance, physical_device, index);
  // #if defined(_WIN32)
  //   (void)instance;
  //   return vkGetPhysicalDeviceWin32PresentationSupportKHR(physical_device, index);
  // #else
  //   static_assert(false, "Other platforms other than windows not supported currently.");
  // #endif
}

namespace vk_callbacks {

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    [[maybe_unused]] void* user_data) noexcept {

  const char* prepend = nullptr;
  if (type >= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
    prepend = "PERFORMANCE";
  } else if (type >= VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
    prepend = "VALIDATION";
  } else if (type >= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
    prepend = "GENERAL";
  }

  static constexpr char validation_message[] = "<%s> :|vulkan|: %s";
  if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    log_error(validation_message, prepend, callback_data->pMessage);
  } else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    log_warn(validation_message, prepend, callback_data->pMessage);
  } else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    log_info(validation_message, prepend, callback_data->pMessage);
  } else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    log_debug(validation_message, prepend, callback_data->pMessage);
  }
  return VK_FALSE;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(
    VkDebugReportFlagsEXT, // flags
    VkDebugReportObjectTypeEXT object_type,
    uint64_t,    // object
    size_t,      // location
    int32_t,     // message_code
    const char*, // player_prefix
    const char* message,
    void*) // user_data
{
  log_error("[vulkan] Debug report from ObjectType: %i\nMessage: %s\n", object_type, message);
  return VK_FALSE;
}

} // namespace vk_callbacks

// globals
static VkAllocationCallbacks* allocator_callback = nullptr;
static VkInstance instance                       = VK_NULL_HANDLE;
static VkDebugUtilsMessengerEXT debug_messenger  = VK_NULL_HANDLE;
static VkPhysicalDevice physical_device          = VK_NULL_HANDLE;
static VkDevice device                           = VK_NULL_HANDLE;
static uint32_t queue_family                     = (uint32_t)-1;
static VkQueue queue                             = VK_NULL_HANDLE;
static VkDebugReportCallbackEXT debug_report     = VK_NULL_HANDLE;
static VkPipelineCache pipeline_cache            = VK_NULL_HANDLE;
static VkDescriptorPool descriptor_pool          = VK_NULL_HANDLE;
static Linear_Allocator arena                    = { mega_bytes(1) };

constexpr static int swapchain_min_image_count = 2;

namespace vk_state {
static bool swapchain_rebuild = false;
}

static bool
    is_extensions_available(const VkExtensionProperties* properties, u32 properties_count, const char* extension) {
  for (u32 i = 0; i < properties_count; ++i) {
    const VkExtensionProperties& p = properties[i];
    if (strcmp(p.extensionName, extension) == 0) return true;
  }
  return false;
}

void setup_vulkan(const char** instance_extensions_glfw, u32 instance_extensions_count) {
  instance = init_gpu(arena);

  // select physical device.
  {
    u32 gpu_count = 0;
    vk_check(vkEnumeratePhysicalDevices(instance, &gpu_count, nullptr));
    assert(gpu_count > 0);
    auto gpus = arena.push_array_no_init<VkPhysicalDevice>(gpu_count);
    vk_check(vkEnumeratePhysicalDevices(instance, &gpu_count, gpus));

    for (u32 i = 0; i < gpu_count; ++i) {
      // should save stack here. or create a temporary scratch arena. not sure...
      // @TODO: revisit this.
      VkPhysicalDeviceProperties properties;
      vkGetPhysicalDeviceProperties(gpus[i], &properties);
      VkPhysicalDeviceShaderDrawParametersFeatures shader_draw_parameters_feature = {};
      shader_draw_parameters_feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
      shader_draw_parameters_feature.pNext = nullptr;
      shader_draw_parameters_feature.shaderDrawParameters = VK_TRUE;

      VkPhysicalDeviceFeatures2 features = {};
      features.sType                     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
      features.pNext                     = &shader_draw_parameters_feature;
      vkGetPhysicalDeviceFeatures2(gpus[i], &features);

      if (shader_draw_parameters_feature.shaderDrawParameters != VK_TRUE) continue;
      if (features.features.geometryShader != VK_TRUE) continue;

      u32 extension_count = 0;
      vkEnumerateDeviceExtensionProperties(gpus[i], nullptr, &extension_count, nullptr);
      assert(extension_count > 0);
      VkExtensionProperties* available_extensions = arena.push_array_no_init<VkExtensionProperties>(extension_count);
      vkEnumerateDeviceExtensionProperties(gpus[i], nullptr, &extension_count, available_extensions);
      u32 j = 0;
      for (; j < extension_count; ++j) {
        auto& props = available_extensions[j];
        if (!strcmp(props.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) break;
      }

      // did not find
      if (j == extension_count) continue;

      // queue family index
      u32 probable_queue_fam = (u32)-1;
      {
        u32 count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(gpus[i], &count, nullptr);
        assert(count > 0);
        auto queues = arena.push_array_no_init<VkQueueFamilyProperties>(count);
        vkGetPhysicalDeviceQueueFamilyProperties(gpus[i], &count, queues);
        for (u32 j = 0; j < count; ++j) {
          if ((queues[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
              vk_get_physical_device_present_support(instance, gpus[i], j) == VK_TRUE) {
            probable_queue_fam = j;
            assert(queues[j].queueFlags & VK_QUEUE_TRANSFER_BIT);
            break;
          }
        }

        if (probable_queue_fam == (u32)-1) {
          continue;
        }
      }

      if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        physical_device = gpus[i];
        queue_family    = probable_queue_fam;
        break;
      }
    }

    // nothing was selected
    if (physical_device == VK_NULL_HANDLE) {
      physical_device = gpus[0];
      u32 count       = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, nullptr);
      assert(count > 0);
      auto queues = arena.push_array_no_init<VkQueueFamilyProperties>(count);
      vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, queues);
      for (u32 i = 0; i < count; ++i) {
        if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
            vk_get_physical_device_present_support(instance, physical_device, i) == VK_TRUE) {
          queue_family = i;
          assert(queues[i].queueFlags & VK_QUEUE_TRANSFER_BIT);
          break;
        }
      }
      assert(queue_family == (u32)-1);
    }
    arena.clear();
  }

  // create a device
  {
    const char** device_extentions               = arena.push_array_no_init<const char*>(2);
    u32 device_extensions_count                  = 0;
    device_extentions[device_extensions_count++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    u32 extension_count                          = 0;
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr);
    assert(extension_count > 0);
    VkExtensionProperties* available_extensions = arena.push_array_no_init<VkExtensionProperties>(extension_count);
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, available_extensions);
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
    if (is_extensions_available(available_extensions, extension_count, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
      device_extensions[device_extensions_count++] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
#endif
    const float queue_priority         = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {};
    queue_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex        = queue_family;
    queue_info.queueCount              = 1;
    queue_info.pQueuePriorities        = &queue_priority;

    VkDeviceCreateInfo create_info      = {};
    create_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount    = 1;
    create_info.pQueueCreateInfos       = &queue_info;
    create_info.enabledExtensionCount   = device_extensions_count;
    create_info.ppEnabledExtensionNames = device_extentions;
    vk_check(vkCreateDevice(physical_device, &create_info, allocator_callback, &device));
    vkGetDeviceQueue(device, queue_family, 0, &queue);
    arena.clear();
  }

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
    vk_check(vkCreateDescriptorPool(device, &pool_info, allocator_callback, &descriptor_pool));
  }
}

void cleanup_vulkan() {
  vkDestroyDescriptorPool(device, descriptor_pool, allocator_callback);
  vkDestroyDevice(device, allocator_callback);
#if VK_ENABLE_VALIDATION
  //auto vkDestroyDebugUtilsMessengerEXT =
  //    (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
  //assert(vkDestroyDebugUtilsMessengerEXT);
  //vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, allocator_callback);

  //auto vkDestroyDebugReportCallbackEXT =
  //    (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
  //assert(vkDestroyDebugReportCallbackEXT);
  //vkDestroyDebugReportCallbackEXT(instance, debug_report, allocator_callback);
#endif
  // we don't own this.
  vkDestroyInstance(instance, allocator_callback);
}

namespace to_remove {
ImGui_ImplVulkanH_Window main_window_imgui_impl = {};
}

// I think you are only able to create one here because we need to manage people's expectations.
Window create_surface(GLFWwindow* window) {
  // Create Window Surface
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  log_debug("made it here maybe");

  // VkWin32SurfaceCreateInfoKHR create_info {};
  // create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  // create_info.pNext = nullptr;
  // create_info.hinstance = NULL;
  // create_info.hwnd = glfwGetWin32Window(window);
  // vk_check(vkCreateWin32SurfaceKHR(instance, &create_info, allocator_callback, &surface));

  vk_check(glfwCreateWindowSurface(instance, window, allocator_callback, &surface));

  // Create Framebuffers
  int width, height;
  glfwGetFramebufferSize(window, &width, &height);
  ImGui_ImplVulkanH_Window* wd = &to_remove::main_window_imgui_impl;
  log_debug("made it here maybe");
  // SetupVulkanWindow(wd, surface, w, h);
  wd->Surface = surface;

  VkBool32 res = VK_FALSE;
  // I think we have to call this or we get a validation error.
  vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, queue_family, wd->Surface, &res);
  assert(res == VK_TRUE);

  log_debug("made it here maybe");
  // Select Surface Format
  const VkFormat requestSurfaceImageFormat[]     = { VK_FORMAT_B8G8R8A8_UNORM,
                                                     VK_FORMAT_R8G8B8A8_UNORM,
                                                     VK_FORMAT_B8G8R8_UNORM,
                                                     VK_FORMAT_R8G8B8_UNORM };
  const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
  wd->SurfaceFormat                              = ImGui_ImplVulkanH_SelectSurfaceFormat(
      physical_device,
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
      ImGui_ImplVulkanH_SelectPresentMode(physical_device, wd->Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));
  // printf("[vulkan] Selected PresentMode = %d\n", wd->PresentMode);

  // Create SwapChain, RenderPass, Framebuffer, etc.
  IM_ASSERT(swapchain_min_image_count >= 2);
  ImGui_ImplVulkanH_CreateOrResizeWindow(
      instance,
      physical_device,
      device,
      wd,
      queue_family,
      allocator_callback,
      width,
      height,
      swapchain_min_image_count);

  // Setup Platform/Renderer backends
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance                  = instance;
  init_info.PhysicalDevice            = physical_device;
  init_info.Device                    = device;
  init_info.QueueFamily               = queue_family;
  init_info.Queue                     = queue;
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

  vk_check(vkDeviceWaitIdle(device));
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplVulkanH_DestroyWindow(instance, device, &to_remove::main_window_imgui_impl, allocator_callback);
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
          physical_device,
          device,
          &to_remove::main_window_imgui_impl,
          queue_family,
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
  VkResult err                          = vkQueuePresentKHR(queue, &info);
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
      device,
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
        device,
        1,
        &fd->Fence,
        VK_TRUE,
        UINT64_MAX); // wait indefinitely instead of periodically checking
    vk_check(err);

    err = vkResetFences(device, 1, &fd->Fence);
    vk_check(err);
  }
  {
    err = vkResetCommandPool(device, fd->CommandPool, 0);
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
    err = vkQueueSubmit(queue, 1, &info, fd->Fence);
    vk_check(err);
  }
}

} // namespace draw
