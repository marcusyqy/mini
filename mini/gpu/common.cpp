#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include "log.hpp"
#include <cassert>

void vk_check_impl(VkResult err) {
  if (err == VK_SUCCESS) return;
  log_error("[vulkan] Error: VkResult = %d", string_VkResult(err));
  assert(err < 0);
}

