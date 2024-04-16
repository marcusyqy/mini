#include "log.hpp"
#include <cassert>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

void VK_CHECK(VkResult err) {
  if (err == VK_SUCCESS) return;
  log_error("[vulkan] Error: VkResult = %d", string_VkResult(err));
  assert(err < 0);
}
