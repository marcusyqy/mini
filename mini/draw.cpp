#include "draw.hpp"
#include "log.hpp"

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

#if defined(_WIN32)
#include <windows.h>
#undef max
#include <vulkan/vulkan_win32.h>
#endif

// for imgui 
// @TODO: remove this. (just use this for now).
#include "imgui_impl_vulkan.h"

// std libs
#include "mem.hpp"
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
#if defined(_WIN32)
  (void)instance;
  return vkGetPhysicalDeviceWin32PresentationSupportKHR(physical_device, index);
#else
  static_assert(false, "Other platforms other than windows not supported currently.");
#endif
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
static Vk_Arena arena                            = {};

static bool
    is_extensions_available(const VkExtensionProperties* properties, u32 properties_count, const char* extension) {
  for (u32 i = 0; i < properties_count; ++i) {
    const VkExtensionProperties& p = properties[i];
    if (strcmp(p.extensionName, extension) == 0) return true;
  }
  return false;
}

void setup_vulkan(const char** instance_extensions_glfw, u32 instance_extensions_count) {
  VkApplicationInfo app_info  = {};
  app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pNext              = nullptr;
  app_info.pApplicationName   = "mini_app"; // ??
  app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 0);
  app_info.pEngineName        = "mini";
  app_info.engineVersion      = VK_MAKE_VERSION(0, 0, 0);
  app_info.apiVersion         = vk_constants::api_version;

  const char* layers = nullptr;
  u32 layer_count    = 0;
#if VK_ENABLE_VALIDATION
  vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
  VkLayerProperties* available_layers = available_layers = arena.push_array_no_init<VkLayerProperties>(layer_count);
  vkEnumerateInstanceLayerProperties(&layer_count, available_layers);

  for (u32 i = 0; i < layer_count; ++i) {
    if (!strcmp(vk_constants::validation_layer, available_layers[i].layerName)) {
      layers      = vk_constants::validation_layer;
      layer_count = 1;
      break;
    }
  }
  arena.clear();
#endif

  // create instance.
  {
    VkInstanceCreateInfo create_info = {};
    create_info.sType                = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pNext                = nullptr;
    create_info.flags                = 0;
    create_info.pApplicationInfo     = &app_info;
    create_info.enabledLayerCount    = layer_count;
    create_info.ppEnabledLayerNames  = &layers;

    u32 properties_count = 0;
    // Enumerate available extensions
    vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
    auto properties = arena.push_array_no_init<VkExtensionProperties>(properties_count);
    vk_check(vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, properties));

    // Enable required extensions
    auto instance_extensions = arena.push_array_no_init<const char*>(instance_extensions_count + 4);
    // first memcpy everything
    memcpy(&instance_extensions, &instance_extensions_glfw, sizeof(const char*) * instance_extensions_count);

    if (is_extensions_available(properties, properties_count, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
      instance_extensions[instance_extensions_count++] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
    if (is_extensions_available(properties, properties_count, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
      instance_extensions[instance_extensions_count++] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
      create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
#endif

    // Enabling validation layers
    const char* layers[]            = { "VK_LAYER_KHRONOS_validation" };
    create_info.enabledLayerCount   = 1;
    create_info.ppEnabledLayerNames = layers;

#if VK_ENABLE_VALIDATION
    instance_extensions[instance_extensions_count++] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
    instance_extensions[instance_extensions_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
#endif

    // Create Vulkan Instance
    create_info.enabledExtensionCount   = instance_extensions_count;
    create_info.ppEnabledExtensionNames = instance_extensions;
    vk_check(vkCreateInstance(&create_info, allocator_callback, &instance));

#if VK_ENABLE_VALIDATION
    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = {};
    {
      debug_messenger_create_info.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
      debug_messenger_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
      debug_messenger_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
      debug_messenger_create_info.pfnUserCallback = &vk_callbacks::debug_callback;
      debug_messenger_create_info.pUserData       = nullptr; // Optional
    }

    auto vkCreateDebugUtilsMessengerEXT =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    assert(vkCreateDebugUtilsMessengerEXT);
    vk_check(vkCreateDebugUtilsMessengerEXT(instance, &debug_messenger_create_info, nullptr, &debug_messenger));

    // Setup the debug report callback
    auto vkCreateDebugReportCallbackEXT =
        (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
    assert(vkCreateDebugReportCallbackEXT);

    VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
    debug_report_ci.sType                              = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    debug_report_ci.flags =
        VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    debug_report_ci.pfnCallback = vk_callbacks::debug_report;
    debug_report_ci.pUserData   = nullptr;
    vk_check(vkCreateDebugReportCallbackEXT(instance, &debug_report_ci, allocator_callback, &debug_report));
#endif

    arena.clear();
  }

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
  auto vkDestroyDebugUtilsMessengerEXT =
      (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
  assert(vkDestroyDebugUtilsMessengerEXT);
  vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, allocator_callback);

  auto vkDestroyDebugReportCallbackEXT =
      (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
  assert(vkDestroyDebugReportCallbackEXT);
  vkDestroyDebugReportCallbackEXT(instance, debug_report, allocator_callback);
#endif
  vkDestroyInstance(instance, allocator_callback);
}

namespace to_remove {
ImGui_ImplVulkanH_Window main_window_imgui_impl;
}

// I think you are only able to create one here because we need to manage people's expectations.
Window create_surface(GLFWwindow* window) {
  // Create Window Surface
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  vk_check(glfwCreateWindowSurface(instance, window, allocator_callback, &surface));

  // Create Framebuffers
  int w, h;
  glfwGetFramebufferSize(window, &w, &h);
  ImGui_ImplVulkanH_Window* wd = &to_remove::main_window_imgui_impl;
  // SetupVulkanWindow(wd, surface, w, h);
  wd->Surface = surface;

  VkBool32 res = VK_FALSE;
  // I think we have to call this or we get a validation error.
  vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, queue_family, wd->Surface, &res);
  assert(res == VK_TRUE);

  VkSwapchainKHR swapchain = VK_NULL_HANDLE;

  return { window, surface, swapchain };
}

} // namespace draw