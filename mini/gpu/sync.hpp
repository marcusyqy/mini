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

struct Delay_Info {
  GPU_Resource_Type resource_type;
  void* resource_ptr = nullptr;
  VkInstance instance = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  const VkAllocationCallbacks* allocator = nullptr;
  // probably shouldn't be accessed by the user?
  Delay_Info* next; 
  Delay_Info* prev; 
  Delay_Info* pad; // we are just using this for now to pad
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
  
  Delay_Info* head = nullptr;
  Delay_Info* storage = nullptr;
};