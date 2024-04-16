#include "device.hpp"
#include "common.hpp"
#include "core/memory.hpp"
#include "defs.hpp"
#include "log.hpp"
#include <GLFW/glfw3.h>

// Check if there is debug.
#if !defined(DEBUG)
#define VK_DEBUG 1
#else
#define VK_DEBUG 0
#endif // DEBUG

#if defined(_WIN32) || defined(WIN32)
#define GPU_PLATFORM_SURFACE "VK_KHR_win32_surface"
#else
static_assert(false, "Other platforms not supported currently.");
#endif

static constexpr auto VK_API_VERSION                  = VK_API_VERSION_1_3;
static constexpr const char* VK_VALIDATION_LAYER      = "VK_LAYER_KHRONOS_validation";
static constexpr const char* VK_REQUIRED_EXTENSIONS[] = {
  "VK_KHR_surface",
  GPU_PLATFORM_SURFACE,
#if VK_DEBUG
  VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
  VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
};

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

/// Not sure if this will be used?
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

static bool is_instance_extensions_available(
    const VkExtensionProperties* properties,
    u32 properties_count,
    const char* extension) {
  for (u32 i = 0; i < properties_count; ++i) {
    const VkExtensionProperties& p = properties[i];
    if (strcmp(p.extensionName, extension) == 0) return true;
  }
  return false;
}

// we probably need to wrap this somewhere
static VkInstance instance              = VK_NULL_HANDLE;
static VkAllocationCallbacks* allocator = nullptr; // can point to something meaningful.

VkInstance init_gpu(Linear_Allocator& arena) {

  VkApplicationInfo app_info  = {};
  app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pNext              = nullptr;
  app_info.pApplicationName   = "mini engine";
  app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 0);
  app_info.pEngineName        = "mini gpu";
  app_info.engineVersion      = VK_MAKE_VERSION(0, 0, 0);
  app_info.apiVersion         = VK_API_VERSION;

  const char* layers = nullptr;
  u32 layer_count    = 0;
#if VK_DEBUG
  vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
  VkLayerProperties* available_layers = available_layers = arena.push_array_no_init<VkLayerProperties>(layer_count);
  vkEnumerateInstanceLayerProperties(&layer_count, available_layers);

  for (u32 i = 0; i < layer_count; ++i) {
    if (!strcmp(VK_VALIDATION_LAYER, available_layers[i].layerName)) {
      layers      = VK_VALIDATION_LAYER;
      layer_count = 1;
      break;
    }
  }
  arena.clear();
#endif

  // create instance.
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
  VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, properties));

  u32 instance_extensions_count = ARRAY_SIZE(VK_REQUIRED_EXTENSIONS);
  // Enable required extensions
  auto instance_extensions = arena.push_array_no_init<const char*>(instance_extensions_count + 4);
  // first memcpy everything
  memcpy(instance_extensions, VK_REQUIRED_EXTENSIONS, sizeof(const char*) * instance_extensions_count);

  if (is_instance_extensions_available(
          properties,
          properties_count,
          VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
    instance_extensions[instance_extensions_count++] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
  if (is_instance_extensions_available(properties, properties_count, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
    instance_extensions[instance_extensions_count++] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
    create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
  }
#endif

  // Enabling validation layers
  const char* validation_layers[] = { "VK_LAYER_KHRONOS_validation" };
  create_info.enabledLayerCount   = 1;
  create_info.ppEnabledLayerNames = validation_layers;

#if VK_ENABLE_VALIDATION
  instance_extensions[instance_extensions_count++] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
  instance_extensions[instance_extensions_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
#endif

  // Create Vulkan Instance
  create_info.enabledExtensionCount   = instance_extensions_count;
  create_info.ppEnabledExtensionNames = instance_extensions;
  VK_CHECK(vkCreateInstance(&create_info, allocator, &instance));

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
  VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance, &debug_messenger_create_info, nullptr, &debug_messenger));

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
  VK_CHECK(vkCreateDebugReportCallbackEXT(instance, &debug_report_ci, allocator_callback, &debug_report));
#endif

  arena.clear();

  /// TODO: use required_extensions instead.
  // u32 extension_count     = {};
  // const char** extensions = glfwGetRequiredInstanceExtensions(&extension_count);
  return instance;
}

void cleanup_gpu() {}

Device create_device(Device_Properties properties) { return {}; }

VkSurfaceKHR platform_create_surface(GLFWwindow* window) {
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  glfwCreateWindowSurface(instance, window, allocator, &surface);
  return surface;
}
