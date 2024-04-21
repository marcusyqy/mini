#include "device.hpp"
#include "common.hpp"
#include "core/memory.hpp"
#include "defs.hpp"
#include "log.hpp"
#include <GLFW/glfw3.h>
#include <cstring>

#define DEBUG

// Check if there is debug.
#if defined(DEBUG)
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

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) noexcept {
  UNUSED_VAR(user_data);

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
static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_report(
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

static VkBool32
    vk_platform_get_physical_device_present_support(VkInstance instance, VkPhysicalDevice physical_device, u32 index) {
  return glfwGetPhysicalDevicePresentationSupport(instance, physical_device, index);
  // #if defined(_WIN32)
  //   (void)instance;
  //   return vkGetPhysicalDeviceWin32PresentationSupportKHR(physical_device, index);
  // #else
  //   static_assert(false, "Other platforms other than windows not supported currently.");
  // #endif
}

// we probably need to wrap this somewhere
static VkInstance instance                      = VK_NULL_HANDLE;
static VkAllocationCallbacks* allocator         = nullptr; // can point to something meaningful.
static VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
static VkDebugReportCallbackEXT debug_report    = VK_NULL_HANDLE;
static bool vk_debug_layers_present             = false;

VkInstance init_gpu_instance(Temp_Linear_Allocator arena) {
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
      layers                  = VK_VALIDATION_LAYER;
      layer_count             = 1;
      vk_debug_layers_present = true;
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

#if VK_DEBUG
  instance_extensions[instance_extensions_count++] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
  instance_extensions[instance_extensions_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
#endif

  // Create Vulkan Instance
  create_info.enabledExtensionCount   = instance_extensions_count;
  create_info.ppEnabledExtensionNames = instance_extensions;
  VK_CHECK(vkCreateInstance(&create_info, allocator, &instance));

#if VK_DEBUG // need to check if there even contains the debug layers
  if (vk_debug_layers_present) {
    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = {};
    debug_messenger_create_info.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_messenger_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_messenger_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_messenger_create_info.pfnUserCallback = &vk_debug_callback;
    debug_messenger_create_info.pUserData       = nullptr; // Optional

    auto vkCreateDebugUtilsMessengerEXT =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    assert(vkCreateDebugUtilsMessengerEXT);
    VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance, &debug_messenger_create_info, allocator, &debug_messenger));

    // Setup the debug report callback
    auto vkCreateDebugReportCallbackEXT =
        (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
    assert(vkCreateDebugReportCallbackEXT);

    VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
    debug_report_ci.sType                              = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    debug_report_ci.flags =
        VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    debug_report_ci.pfnCallback = vk_debug_report;
    debug_report_ci.pUserData   = nullptr;
    VK_CHECK(vkCreateDebugReportCallbackEXT(instance, &debug_report_ci, allocator, &debug_report));
  } else {
    log_error("Debug layers not present for vulkan even though DEBUG is true!");
  }
#endif

  arena.clear();

  // volkLoadInstanceOnly(instance);
  return instance;
}

void cleanup_gpu_instance() {
#if VK_DEBUG
  auto vkDestroyDebugUtilsMessengerEXT =
      (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
  assert(vkDestroyDebugUtilsMessengerEXT);
  vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, allocator);

  auto vkDestroyDebugReportCallbackEXT =
      (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
  assert(vkDestroyDebugReportCallbackEXT);
  vkDestroyDebugReportCallbackEXT(instance, debug_report, allocator);
#endif
  vkDestroyInstance(instance, allocator);
  // volkFinalize();
}

Device create_device(Temp_Linear_Allocator arena) {
  /// TODO: add maybe properties to check? For rendering, for compute...
  Device device   = {};
  device.instance = instance;
  device.allocator = allocator;

  VkDevice& logical_device          = device.logical;
  VkPhysicalDevice& physical_device = device.physical;
  VkQueue& queue                    = device.queue;
  u32& queue_family                 = device.queue_family;

  u32 gpu_count = 0;
  VK_CHECK(vkEnumeratePhysicalDevices(instance, &gpu_count, nullptr));
  assert(gpu_count > 0);
  auto gpus = arena.push_array_no_init<VkPhysicalDevice>(gpu_count);
  VK_CHECK(vkEnumeratePhysicalDevices(instance, &gpu_count, gpus));

  constexpr auto required_version = VK_API_VERSION_1_3;

  // vulkan 1.3 features
  VkPhysicalDeviceVulkan13Features features13{};
  features13.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  features13.dynamicRendering = true;
  features13.synchronization2 = true;

  // vulkan 1.2 features
  VkPhysicalDeviceVulkan12Features features12{};
  features12.sType               = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  features12.bufferDeviceAddress = true;
  features12.descriptorIndexing  = true;

  auto scratch = arena.save();
  for (u32 i = 0; i < gpu_count; ++i) {
    // should save stack here. or create a temporary scratch arena. not sure...
    /// @TODO: revisit this.
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(gpus[i], &properties);
    if (properties.apiVersion < required_version) continue;

    // Rendering features here.
    VkPhysicalDeviceShaderDrawParametersFeatures shader_draw_parameters_feature = {};
    shader_draw_parameters_feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
    shader_draw_parameters_feature.pNext = nullptr;
    shader_draw_parameters_feature.shaderDrawParameters = VK_TRUE;

    // vulkan 1.2 feature
    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType               = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.bufferDeviceAddress = VK_TRUE;
    features12.descriptorIndexing  = VK_TRUE;
    features12.pNext               = &shader_draw_parameters_feature;

    // vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = true;
    features13.synchronization2 = VK_TRUE;
    features13.pNext            = &features12;

    VkPhysicalDeviceFeatures2 features = {};
    features.sType                     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext                     = &features13;

    vkGetPhysicalDeviceFeatures2(gpus[i], &features);

    // check for necessary features here.
    if (shader_draw_parameters_feature.shaderDrawParameters != VK_TRUE) continue;
    if (features13.dynamicRendering != VK_TRUE) continue;
    if (features13.synchronization2 != VK_TRUE) continue;
    if (features12.descriptorIndexing != VK_TRUE) continue;
    if (features12.bufferDeviceAddress != VK_TRUE) continue;
    if (features.features.geometryShader != VK_TRUE) continue;

    u32 extension_count = 0;
    vkEnumerateDeviceExtensionProperties(gpus[i], nullptr, &extension_count, nullptr);
    assert(extension_count > 0);
    VkExtensionProperties* available_extensions = scratch.push_array_no_init<VkExtensionProperties>(extension_count);
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
      auto queues = scratch.push_array_no_init<VkQueueFamilyProperties>(count);
      vkGetPhysicalDeviceQueueFamilyProperties(gpus[i], &count, queues);
      for (u32 j = 0; j < count; ++j) {
        if ((queues[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            vk_platform_get_physical_device_present_support(instance, gpus[i], j) == VK_TRUE) {
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

    scratch.clear();
  }

  scratch.clear();

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
          vk_platform_get_physical_device_present_support(instance, physical_device, i) == VK_TRUE) {
        queue_family = i;
        assert(queues[i].queueFlags & VK_QUEUE_TRANSFER_BIT);
        break;
      }
    }
    assert(queue_family == (u32)-1);
  }

  arena.clear();

  // create a device
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

  VK_CHECK(vkCreateDevice(physical_device, &create_info, allocator, &logical_device));
  vkGetDeviceQueue(logical_device, queue_family, 0, &queue);
  arena.clear();

  // volkLoadDevice(device);
  return device;
}

void destroy_device(Device device) { vkDestroyDevice(device.logical, allocator); }

VkSurfaceKHR platform_create_vk_surface(GLFWwindow* window) {
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  /// Creating with win32.
  // VkWin32SurfaceCreateInfoKHR create_info {};
  // create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  // create_info.pNext = nullptr;
  // create_info.hinstance = NULL;
  // create_info.hwnd = glfwGetWin32Window(window);
  // vk_check(vkCreateWin32SurfaceKHR(instance, &create_info, allocator_callback, &surface));
  VK_CHECK(glfwCreateWindowSurface(instance, window, allocator, &surface));
  return surface;
}

void platform_destroy_vk_surface(VkSurfaceKHR surface) { vkDestroySurfaceKHR(instance, surface, allocator); }
