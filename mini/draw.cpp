#include "draw.hpp"
#include "log.hpp"

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

// for imgui
#include "imgui_impl_vulkan.h"

// std libs
#include "mem.hpp"
#include <cassert>
#include <cstdio>

#define VK_ENABLE_VALIDATION 1

namespace draw {

namespace vk_constants {
constexpr auto api_version                  = VK_API_VERSION_1_3;
constexpr const char* validation_layer      = "VK_LAYER_KHRONOS_validation";

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

  static constexpr char validation_message[] = "<{}> :|vulkan|: {}";
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
static Stack_Allocator<1 << 15> arena            = {};

static bool
    is_extensions_available(const VkExtensionProperties* properties, u32 properties_count, const char* extension) {
  for (u32 i = 0; i < properties_count; ++i) {
    const VkExtensionProperties& p = properties[i];
    if (strcmp(p.extensionName, extension) == 0) return true;
  }
  return false;
}

Vk_Vars setup_vulkan(const char** instance_extensions_glfw, u32 instance_extensions_count) {
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
    const char* layers[]                             = { "VK_LAYER_KHRONOS_validation" };
    create_info.enabledLayerCount                    = 1;
    create_info.ppEnabledLayerNames                  = layers;

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
      debug_messenger_create_info.pfnUserCallback = vk_callbacks::debug_callback;
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

  log_info("nothing happened that made us crash");

  return {};
}

void cleanup_vulkan() {
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

} // namespace draw
