#pragma once
#include <vulkan/vulkan.h>

void vk_check_impl(VkResult err); 

#define VK_CHECK(err) vk_check_impl(err)
