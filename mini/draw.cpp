#include "draw.hpp"
#include "log.hpp"

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

// for imgui
#include "imgui_impl_vulkan.h"

// std libs
#include <cassert>
#include <cstdio>

namespace draw {

namespace vk_globals {

static VkAllocationCallbacks* allocator      = nullptr;
static VkInstance instance                   = VK_NULL_HANDLE;
static VkPhysicalDevice physical_device      = VK_NULL_HANDLE;
static VkDevice device                       = VK_NULL_HANDLE;
static uint32_t queue_family                 = (uint32_t)-1;
static VkQueue queue                         = VK_NULL_HANDLE;
static VkDebugReportCallbackEXT debug_report = VK_NULL_HANDLE;
static VkPipelineCache pipeline_cache        = VK_NULL_HANDLE;
static VkDescriptorPool descriptor_pool      = VK_NULL_HANDLE;

} // namespace vk_globals

namespace callbacks {
static void check_vk_result(VkResult err) {
  if (err == 0) return;
  log_error("[vulkan] Error: VkResult = %d", err);
  assert(err < 0);
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

} // namespace callbacks

Vk_Vars setup_vulkan(const char** extensions, u32 count) { return {}; }

} // namespace draw
