#pragma once
#include <vulkan/vulkan.h>
#include "vma/vk_mem_alloc.h"

void VK_CHECK(VkResult err);

struct Image {
  VkImage image;
  VkImageView view;
  VmaAllocation allocation;
  VkExtent3D extent;
  VkFormat format;
};