#include "memory.hpp"
#include <cstdlib>
#include <cstring>

void default_allocator_proc(Allocation_Parameters* params, Allocation_Result* result) {
  assert(result);
  assert(params);

  result->info = Allocation_Err::none;

  switch (params->alloc_instruction) {
    case Allocation_Op::alloc: {
      result->memory = malloc(params->size);
      memset(result->memory, 0, params->size);
    } break;
    case Allocation_Op::resize: {
      result->memory = realloc(params->memory, params->size);
      if (result->memory != params->memory) memset(result->memory, 0, params->size);
      else if (params->size > params->old_size)
        memset(result->memory, 0, params->size - params->old_size);
    } break;
    case Allocation_Op::alloc_no_zero: {
      result->memory = malloc(params->size);
    } break;
    case Allocation_Op::resize_no_zero: {
      result->memory = realloc(params->memory, params->size);
    } break;
    case Allocation_Op::free: {
      free(params->memory);
      return;
    } break;
  }

  if (result->memory == nullptr) result->info = Allocation_Err::out_of_memory;
}

Allocation_Result Allocator::allocate(u64 size, u64 alignment) {
  assert(alloc_proc);
  Allocation_Parameters params = {};
  params.alloc_instruction     = Allocation_Op::alloc;
  params.user_ptr              = user_ptr;
  params.size                  = size;
  params.alignment             = alignment;

  Allocation_Result result = {};
  alloc_proc(&params, &result);
  return result;
}

Allocation_Result Allocator::allocate_no_zero(u64 size, u64 alignment) {
  assert(alloc_proc);
  Allocation_Parameters params = {};
  params.alloc_instruction     = Allocation_Op::alloc_no_zero;
  params.user_ptr              = user_ptr;
  params.size                  = size;
  params.alignment             = alignment;

  Allocation_Result result = {};
  alloc_proc(&params, &result);
  return result;
}

Allocation_Err Allocator::free(void* memory) {
  assert(alloc_proc);
  Allocation_Parameters params = {};
  params.alloc_instruction     = Allocation_Op::free;
  params.user_ptr              = user_ptr;
  params.memory                = memory;

  Allocation_Result result = {};
  alloc_proc(&params, &result);
  return result.info;
}

Allocation_Result Allocator::realloc(void* memory, u64 size, u64 alignment, u64 old_size) {
  assert(alloc_proc);
  Allocation_Parameters params = {};
  params.alloc_instruction     = Allocation_Op::alloc_no_zero;
  params.user_ptr              = user_ptr;
  params.memory                = memory;
  params.size                  = size;
  params.alignment             = alignment;
  params.old_size              = old_size;

  Allocation_Result result = {};
  alloc_proc(&params, &result);
  return result;
}

Allocation_Result Allocator::realloc_no_zero(void* memory, u64 size, u64 alignment, u64 old_size) {
  assert(alloc_proc);
  Allocation_Parameters params = {};
  params.alloc_instruction     = Allocation_Op::alloc_no_zero;
  params.user_ptr              = user_ptr;
  params.memory                = memory;
  params.size                  = size;
  params.alignment             = alignment;
  params.old_size              = old_size;

  Allocation_Result result = {};
  alloc_proc(&params, &result);
  return result;
}

void Linear_Allocator_Strategy::init(u8* _buf, u64 _size) {
  buf         = _buf;
  size        = _size;
  prev_offset = 0;
  curr_offset = 0;
}

Allocation_Result Linear_Allocator_Strategy::alloc(u64 size, u64 alignment) {
  assert(is_power_of_two(alignment));
  auto buffer = (uintptr_t)buf;
  auto p      = buffer + (uintptr_t)curr_offset;
  auto a      = (uintptr_t)alignment;

  auto mod = p & (a - 1);
  if (mod != 0) p += a - mod;

  Allocation_Result ret = {};
  auto result           = p + (uintptr_t)size - buffer;
  if (result <= size) {
    assert(result >= 0);
    prev_offset = curr_offset;
    curr_offset = (u64)result;
    ret.memory  = (void*)p;
    ret.info    = Allocation_Err::none;
    return ret;
  }

  ret.memory = nullptr;
  ret.info   = Allocation_Err::out_of_memory;
  return ret;
}

Allocation_Result Linear_Allocator_Strategy::realloc(void* previous, u64 prev_size, u64 size, u64 alignment) {
  u8* old_mem = (u8*)previous;
  assert(is_power_of_two(alignment));

  if (previous == nullptr || prev_size == 0) {
    return alloc(size, alignment);
  } else if (buf <= previous && previous < buf + size) {
    if (buf + prev_offset == old_mem) {
      curr_offset = prev_offset + size;
      return { previous, Allocation_Err::none };
    } else {
      auto new_alloc = alloc(size, alignment);
      if (new_alloc.info == Allocation_Err::out_of_memory) {
        return new_alloc;
      }
      size_t copy_size = prev_size < size ? prev_size : size;
      // Copy across old memory to the new memory
      memmove(new_alloc.memory, previous, copy_size);
      return new_alloc;
    }
  } else {
    return { nullptr, Allocation_Err::out_of_bounds };
  }
}

void Linear_Allocator_Strategy::clear() {
  prev_offset = 0;
  curr_offset = 0;
}

void* Linear_Allocator::push(u64 size, u64 alignment) {
  auto allocation = strategy.alloc(size, alignment);
  if (allocation.memory == nullptr && allocation.info == Allocation_Err::out_of_memory) {
    auto new_allocation = allocator.allocate(sizeof(Node) + page_size, alignof(Node));
    assert(new_allocation.info != Allocation_Err::out_of_memory);
    current->next = (Node*)new_allocation.memory;
    current       = current->next;
    strategy.init(get_stack_ptr(current), page_size);
  } else {
    return allocation.memory;
  }

  allocation = strategy.alloc(size, alignment);
  // may need to grow page here?
  assert(allocation.info != Allocation_Err::out_of_memory);
  return allocation.memory;
}

// need to call destructor for some T
void Linear_Allocator::clear() {
  current = head;
  strategy.init(get_stack_ptr(current), page_size);
}

void Linear_Allocator::free() {
  while (head) {
    auto tmp = head->next;
    allocator.free((void*)tmp);
    head = tmp;
  }
}

Linear_Allocator::Linear_Allocator(u64 _page_size, Allocator _allocator) :
    page_size{ _page_size }, allocator{ _allocator } {
  // simplify logic by adding them here.
  auto allocation = allocator.allocate(sizeof(Node) + page_size, alignof(Node));
  assert(allocation.info != Allocation_Err::out_of_memory);
  head    = (Node*)allocation.memory;
  current = head;
  strategy.init(get_stack_ptr(current), page_size);
}

Linear_Allocator::~Linear_Allocator() { free(); }

Temp_Linear_Allocator Linear_Allocator::save() { return Save_Point{ current, this, strategy }; }

void Linear_Allocator::load(Temp_Linear_Allocator temp) {
  assert(temp.save_point.allocator == this);
  current  = temp.save_point.current;
  strategy = temp.save_point.strategy;
}