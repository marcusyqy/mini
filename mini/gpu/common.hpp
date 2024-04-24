#pragma once
#include "vma/vk_mem_alloc.h"
#include <vulkan/vulkan.h>

void VK_CHECK(VkResult err);

struct Image {
  VkImage image;
  VkImageView view;
  VmaAllocation allocation;
  VkExtent3D extent;
  VkFormat format;
};