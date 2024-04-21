#include "memory.hpp"
#include <cstdlib>

Allocation_Return default_allocator_proc(Allocation_Parameters params) {
  Allocation_Return alloc_ret = {};
  alloc_ret.result            = Allocation_Info::none;

  switch (params.alloc_instruction) {
    case Allocation_Instruction::alloc: {
      alloc_ret.memory = malloc(params.size);
      memset(alloc_ret.memory, 0, params.size);
    } break;
    case Allocation_Instruction::free: {
      free(alloc_ret.memory);
    } break;
    case Allocation_Instruction::resize: {
      alloc_ret.memory = realloc(params.memory, params.size);
    } break;
    case Allocation_Instruction::alloc_no_zero: {
      alloc_ret.memory = malloc(params.size);
    } break;
  }

  return alloc_ret;
}

Allocation_Return Allocator::allocate(u64 size, u64 alignment) {
  assert(alloc_proc);
  Allocation_Parameters params = {};
  params.alloc_instruction     = Allocation_Instruction::alloc;
  params.user_ptr              = user_ptr;
  params.size                  = size;
  params.alignment             = alignment;
  return alloc_proc(params);
}

Allocation_Return Allocator::allocate_no_zero(u32 size, u32 alignment) {
  assert(alloc_proc);
  Allocation_Parameters params = {};
  params.alloc_instruction     = Allocation_Instruction::alloc_no_zero;
  params.user_ptr              = user_ptr;
  params.size                  = size;
  params.alignment             = alignment;
  return alloc_proc(params);
}

Allocation_Info Allocator::free(void* memory) {
  assert(alloc_proc);
  Allocation_Parameters params = {};
  params.alloc_instruction     = Allocation_Instruction::free;
  params.user_ptr              = user_ptr;
  params.memory                = memory;
  auto allocation_info         = alloc_proc(params);
  return allocation_info.result;
}

Allocation_Return Allocator::realloc(void* memory, u32 size, u32 alignment) {
  assert(alloc_proc);
  Allocation_Parameters params = {};
  params.alloc_instruction     = Allocation_Instruction::alloc_no_zero;
  params.user_ptr              = user_ptr;
  params.memory                = memory;
  params.size                  = size;
  params.alignment             = alignment;
  return alloc_proc(params);
}

void Linear_Allocator_Strategy::init(u8* _buf, u64 _size) {
  buf         = _buf;
  size        = _size;
  prev_offset = 0;
  curr_offset = 0;
}

Allocation_Return Linear_Allocator_Strategy::alloc(u64 size, u64 alignment) {
  assert(is_power_of_two(alignment));
  auto buffer = (uintptr_t)buf;
  auto p      = buffer + (uintptr_t)curr_offset;
  auto a      = (uintptr_t)alignment;

  auto mod = p & (a - 1);
  if (mod != 0) p += a - mod;

  Allocation_Return ret = {};
  auto result           = p + (uintptr_t)size - buffer;
  if (result <= size) {
    assert(result >= 0);
    prev_offset = curr_offset;
    curr_offset = (u64)result;
    ret.memory  = (void*)p;
    ret.result  = Allocation_Info::none;
    return ret;
  }

  ret.memory = nullptr;
  ret.result = Allocation_Info::out_of_memory;
  return ret;
}

Allocation_Return Linear_Allocator_Strategy::realloc(void* previous, u64 prev_size, u64 size, u64 alignment) {
  u8* old_mem = (u8*)previous;
  assert(is_power_of_two(alignment));

  if (previous == nullptr || prev_size == 0) {
    return alloc(size, alignment);
  } else if (buf <= previous && previous < buf + size) {
    if (buf + prev_offset == old_mem) {
      curr_offset = prev_offset + size;
      return { previous, Allocation_Info::none };
    } else {
      auto new_alloc = alloc(size, alignment);
      if (new_alloc.result == Allocation_Info::out_of_memory) {
        return new_alloc;
      }
      size_t copy_size = prev_size < size ? prev_size : size;
      // Copy across old memory to the new memory
      memmove(new_alloc.memory, previous, copy_size);
      new_alloc.result = Allocation_Info::relocated_memory;
      return new_alloc;
    }
  } else {
    return { nullptr, Allocation_Info::out_of_bounds };
  }
}

void Linear_Allocator_Strategy::clear() {
  prev_offset = 0;
  curr_offset = 0;
}
