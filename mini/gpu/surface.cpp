#include "surface.hpp"
#include "common.hpp"
#include "core/common.hpp"
#include "device.hpp"
#include <GLFW/glfw3.h>

#if 0 // TODO
static void swapchain_acquire_next_image(Swapchain& swapchain) {
  if (swapchain.semaphore_size == 0) {
    VkSemaphore semaphore;
    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VK_EXPECT_SUCCESS(vkCreateSemaphore(gpu.logical, &semaphore_info, nullptr, &semaphore));
    swapchain.semaphore_pool[swapchain.semaphore_size++] = semaphore;
  }

  assert(swapchain.semaphore_size < Render_Params::MAX_SWAPCHAIN_IMAGES);
  auto semaphore = swapchain.semaphore_pool[--swapchain.semaphore_size];

  VkResult result = vkAcquireNextImageKHR(
      gpu.logical,
      swapchain.handle,
      std::numeric_limits<std::uint64_t>::max(),
      semaphore,
      nullptr,
      &swapchain.current_frame);

  assert(result == VK_SUBOPTIMAL_KHR || result == VK_SUCCESS || result == VK_ERROR_OUT_OF_DATE_KHR);
  if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR) swapchain.out_of_date = true;

  swapchain.semaphore_pool[swapchain.semaphore_size++] = swapchain.image_avail[swapchain.current_frame];
  swapchain.image_avail[swapchain.current_frame]       = semaphore;
}

#endif

static void create_or_reinitialize_swapchain(Temp_Linear_Allocator arena, Device* device, Surface* surface) {
  const auto old_num_images             = surface->num_images;
  VkSurfaceCapabilitiesKHR capabilities = {};
  VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physical, surface->surface, &capabilities));

  if (surface->swapchain != VK_NULL_HANDLE && capabilities.currentExtent.width == surface->width && capabilities.currentExtent.height == surface->height)
    return;

  s16 width  = surface->width;
  s16 height = surface->height;

  if (capabilities.currentExtent.width != UINT32_MAX) {
    width  = capabilities.currentExtent.width;
    height = capabilities.currentExtent.height;
  }

  if (surface->swapchain != VK_NULL_HANDLE && width == surface->width && height == surface->height) return;

  surface->width  = width;
  surface->height = height;

  u32 format_count{};
  vkGetPhysicalDeviceSurfaceFormatsKHR(device->physical, surface->surface, &format_count, nullptr);
  assert(format_count != 0);
  auto formats = arena.push_array_no_init<VkSurfaceFormatKHR>(format_count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(device->physical, surface->surface, &format_count, formats);

  u32 present_mode_count{};
  vkGetPhysicalDeviceSurfacePresentModesKHR(device->physical, surface->surface, &present_mode_count, nullptr);
  auto present_modes = arena.push_array_no_init<VkPresentModeKHR>(present_mode_count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(device->physical, surface->surface, &present_mode_count, present_modes);

  const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM,
                                                 VK_FORMAT_R8G8B8A8_UNORM,
                                                 VK_FORMAT_B8G8R8_UNORM,
                                                 VK_FORMAT_R8G8B8_UNORM };
  bool format_chosen                         = false;
  for (u32 i = 0; i < format_count; ++i) {
    const auto& format = formats[i];

    bool format_matched = format.format == VK_FORMAT_B8G8R8A8_SRGB || format.format == VK_FORMAT_B8G8R8A8_UNORM ||
        format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8_UNORM ||
        format.format == VK_FORMAT_R8G8B8_UNORM;

    if (format_matched && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      surface->format = format;
      format_chosen   = true;
      break;
    }
  }

  if (!format_chosen) surface->format = formats[0];

  VkPresentModeKHR chosen_present_mode = VK_PRESENT_MODE_FIFO_KHR;
  // @NOTE: FIFO for smooth resize but dragging window is laggy
  /*for (u32 i = 0; i < present_mode_count; ++i) {
      const auto& mode = present_modes[i];
      if (mode == VK_PRESENT_MODE_MAILBOX_KHR) chosen_present_mode = mode;
  }*/

  constexpr u32 desired_image_count = 3;
  const auto image_count = clamp(desired_image_count, capabilities.minImageCount, capabilities.maxImageCount);
  // clamp here.
  // if (image_count < capabilities.minImageCount) image_count = capabilities.minImageCount;
  // if (image_count > capabilities.maxImageCount) image_count = capabilities.maxImageCount;

  // Find a supported composite type.
  VkCompositeAlphaFlagBitsKHR composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) {
    composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  } else if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) {
    composite = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
  } else if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) {
    composite = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
  } else if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) {
    composite = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
  }

  VkSwapchainCreateInfoKHR create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, create_info.pNext = nullptr;
  create_info.flags                 = 0;
  create_info.surface               = surface->surface;
  create_info.minImageCount         = image_count;
  create_info.imageFormat           = surface->format.format;
  create_info.imageColorSpace       = surface->format.colorSpace;
  create_info.imageExtent           = { (u32)surface->width, (u32)surface->height };
  create_info.imageArrayLayers      = 1;
  create_info.imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  create_info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
  create_info.queueFamilyIndexCount = 0;
  create_info.pQueueFamilyIndices   = nullptr;
  create_info.preTransform          = capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
               ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
               : capabilities.currentTransform;
  create_info.compositeAlpha        = composite; // VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
  create_info.presentMode           = chosen_present_mode;
  create_info.clipped               = VK_TRUE;
  create_info.oldSwapchain          = surface->swapchain;
  VK_CHECK(vkCreateSwapchainKHR(device->logical, &create_info, nullptr, &surface->swapchain));
  arena.clear();

  // @TODO: erase all resources without waiting for gpu to be idle
  vkDeviceWaitIdle(device->logical);

  // cleanup : required to safely destroy after creating new swapchain.
  if (create_info.oldSwapchain != nullptr) {
    for (u8 i = 0; i < old_num_images; ++i) {
      vkDestroyImageView(device->logical, surface->image_views[i], nullptr);
      vkDestroySemaphore(device->logical, surface->render_done[i], nullptr);
      vkDestroySemaphore(device->logical, surface->image_avail[i], nullptr);
    }
    vkDestroySwapchainKHR(device->logical, create_info.oldSwapchain, nullptr);
  }

  // retrieve images
  VkImage placeholder[Surface::MAX_IMAGES];
  u32 num_images = surface->num_images;
  vkGetSwapchainImagesKHR(device->logical, surface->swapchain, &num_images, nullptr);
  assert(num_images <= Surface::MAX_IMAGES);
  vkGetSwapchainImagesKHR(device->logical, surface->swapchain, &num_images, placeholder);
  surface->num_images = (u8)num_images;

  for (u8 i = 0; i < surface->num_images; ++i) {
    const auto& image = placeholder[i];

    VkImageViewCreateInfo image_view_create_info{};
    image_view_create_info.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_info.image    = image;
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.format   = surface->format.format;

    // image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_R;
    // image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_G;
    // image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_B;
    // image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_A;

    image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    image_view_create_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_create_info.subresourceRange.baseMipLevel   = 0;
    image_view_create_info.subresourceRange.levelCount     = 1;
    image_view_create_info.subresourceRange.baseArrayLayer = 0;
    image_view_create_info.subresourceRange.layerCount     = 1;

    // create image view
    VK_CHECK(vkCreateImageView(device->logical, &image_view_create_info, nullptr, surface->image_views + i));

    // create semaphore
    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VK_CHECK(vkCreateSemaphore(device->logical, &semaphore_info, nullptr, surface->image_avail + i));
    VK_CHECK(vkCreateSemaphore(device->logical, &semaphore_info, nullptr, surface->render_done + i));
  }

  // swapchain_acquire_next_image(swapchain);
}

Surface create_surface(Temp_Linear_Allocator arena, Device& device, GLFWwindow* window, s16 width, s16 height) {
  Surface surface;
  surface.surface = platform_create_vk_surface(window);

  // Short should be enough since it goes up to 32k?
  surface.width  = width;
  surface.height = height;

  VkBool32 res = VK_FALSE;
  // I think we have to call this or we get a validation error.
  vkGetPhysicalDeviceSurfaceSupportKHR(device.physical, device.queue_family, surface.surface, &res);
  assert(res == VK_TRUE);

  create_or_reinitialize_swapchain(arena, &device, &surface);
  return surface;
}

void destroy_surface(Device& device, Surface& surface) {
  vkDeviceWaitIdle(device.logical);
  // destroy swapchain resources
  for (u8 i = 0; i < surface.num_images; ++i) {
    vkDestroyImageView(device.logical, surface.image_views[i], nullptr);
    vkDestroySemaphore(device.logical, surface.image_avail[i], nullptr);
    vkDestroySemaphore(device.logical, surface.render_done[i], nullptr);
  }
  vkDestroySwapchainKHR(device.logical, surface.swapchain, nullptr);

  UNUSED_VAR(device);
  platform_destroy_vk_surface(surface.surface);
}
