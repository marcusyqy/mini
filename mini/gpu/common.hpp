#pragma once
#include <vulkan/vulkan.h>

void VK_CHECK(VkResult err);

struct Buffer {
  VkBuffer buffer;
};

struct Image {
  VkImage buffer;
};
