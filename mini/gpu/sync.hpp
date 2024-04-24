#pragma once
#include "common.hpp"
#include "core/memory.hpp"

enum struct GPU_Resource_Type {
  // Buffer,
  Image,
  Image_View,
  Semaphore,
  Fence,
  Command_Pool,
  Surface,
  Swapchain,
};

/// This struct may be too huge right now? we can probably reduce it more.
/// This instance is always the same and the ... are always the same.
struct Delay_Info {
  void* resource_ptr                               = nullptr;
  VkInstance instance                              = VK_NULL_HANDLE;
  VkDevice device                                  = VK_NULL_HANDLE;
  VmaAllocator allocator                           = VK_NULL_HANDLE;
  const VkAllocationCallbacks* allocator_callbacks = nullptr;
  // probably shouldn't be accessed by the user?
  Delay_Info* next;
  Delay_Info* prev;
  GPU_Resource_Type resource_type;
  u32 pad;
};

Allocator default_delay_queue_allocator();

struct Delay_Queue {
  void flush();

  void push(VkDevice device, VkImageView image_view, const VkAllocationCallbacks* allocator_callbacks);
  void push(VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks* allocator_callbacks);
  void push(VkDevice device, VkFence fence, const VkAllocationCallbacks* allocator_callbacks);
  void push(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* allocator_callbacks);

  Allocator allocator = default_delay_queue_allocator();

private:
  Delay_Info* push_generic();

  Delay_Info* head    = nullptr;
  Delay_Info* storage = nullptr;
};