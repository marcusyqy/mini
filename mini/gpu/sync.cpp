#include "sync.hpp"
#include <cstring>

constexpr auto gpu_delay_info_stack_size = 1000;
static Delay_Info gpu_delay_info_stack[gpu_delay_info_stack_size];
static size_t gpu_delay_info_stack_ptr = 0;

static void default_delay_queue_allocator_proc(Allocation_Parameters* params, Allocation_Result* result) {
  assert(result);
  assert(params);
  assert(params->size == sizeof(Delay_Info));

  result->info = Allocation_Err::none;

  assert(params->op == Allocation_Op::alloc || params->op != Allocation_Op::alloc_no_zero);

  if (gpu_delay_info_stack_ptr >= gpu_delay_info_stack_size) {
    result->info   = Allocation_Err::out_of_memory;
    result->memory = nullptr;
    return;
  }

  result->memory = (void*)(gpu_delay_info_stack + gpu_delay_info_stack_ptr++);
  if (params->op == Allocation_Op::alloc) {
    memset(result->memory, 0, params->size);
  }
}

Allocator default_delay_queue_allocator() {
  Allocator allocator;
  allocator.alloc_proc = default_delay_queue_allocator_proc;
  allocator.user_ptr   = nullptr;
  return allocator;
}

// clang-format off
static void delay_queue_proc(Delay_Info* info) {
  switch (info->resource_type) {
    case GPU_Resource_Type::Image_View:
      vkDestroyImageView(info->device, (VkImageView)info->resource_ptr, info->allocator_callbacks);
      break;
    case GPU_Resource_Type::Semaphore:
      vkDestroySemaphore(info->device, (VkSemaphore)info->resource_ptr, info->allocator_callbacks);
      break;
    case GPU_Resource_Type::Fence: 
      vkDestroyFence(info->device, (VkFence)info->resource_ptr, info->allocator_callbacks); 
      break;
    case GPU_Resource_Type::Swapchain:
      vkDestroySwapchainKHR(info->device, (VkSwapchainKHR)info->resource_ptr, info->allocator_callbacks);
      break;
    default:
      // not implemented yet.
      assert(false);
      break;
  }
}
// clang-format on

void Delay_Queue::flush() {
  auto node = head;
  while (node) {
    delay_queue_proc(node);
    auto next = node->next;
    // push (front) back into storage.
    node->next = storage;
    storage    = node;
    node       = next;
  }
  head = nullptr;
}

Delay_Info* Delay_Queue::push_generic() {
  Delay_Info* result = nullptr;
  if (storage) {
    // pop off the storage list
    result  = storage;
    storage = storage->next;
  } else {
    auto alloc_result = allocator.allocate_no_zero(sizeof(Delay_Info), alignof(Delay_Info));
    assert(alloc_result.info != Allocation_Err::out_of_memory);
    result = (Delay_Info*)alloc_result.memory;
  }
  assert(result);

  // push to the top of the list (when we iterate forward it will be similar to a stack).
  memset(result, 0, sizeof(Delay_Info));
  result->next = head;
  if (head) head->prev = result; // just to preserve next.

  return result;
}

void Delay_Queue::push(VkDevice device, VkImageView image_view, const VkAllocationCallbacks* allocator_callbacks) {
  auto node                 = push_generic();
  node->resource_type       = GPU_Resource_Type::Image_View;
  node->resource_ptr        = image_view;
  node->device              = device;
  node->allocator_callbacks = allocator_callbacks;
}

void Delay_Queue::push(VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks* allocator_callbacks) {
  auto node                 = push_generic();
  node->resource_type       = GPU_Resource_Type::Semaphore;
  node->resource_ptr        = semaphore;
  node->device              = device;
  node->allocator_callbacks = allocator_callbacks;
}

void Delay_Queue::push(VkDevice device, VkFence fence, const VkAllocationCallbacks* allocator_callbacks) {
  auto node                 = push_generic();
  node->resource_type       = GPU_Resource_Type::Fence;
  node->resource_ptr        = fence;
  node->device              = device;
  node->allocator_callbacks = allocator_callbacks;
}

void Delay_Queue::push(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* allocator_callbacks) {
  auto node                 = push_generic();
  node->resource_type       = GPU_Resource_Type::Swapchain;
  node->resource_ptr        = swapchain;
  node->device              = device;
  node->allocator_callbacks = allocator_callbacks;
}
