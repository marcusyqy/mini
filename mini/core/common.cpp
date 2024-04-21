#include "common.hpp"
#include <cassert>

uintptr_t align_forward(uintptr_t ptr, u64 align) {
  assert(is_power_of_two(align));

  uintptr_t a = (uintptr_t)align;

  // Same as (p % a) but faster as 'a' is a power of two
  uintptr_t modulo = ptr & (a - 1);

  if (modulo != 0) {
    // If 'p' address is not aligned, push the address to the
    // next value which is aligned
    ptr += a - modulo;
  }

  return ptr;
}

uintptr_t align_backward(uintptr_t ptr, u64 align) {
  assert(is_power_of_two(align));

  uintptr_t a = (uintptr_t)align;

  // Same as (p % a) but faster as 'a' is a power of two
  uintptr_t modulo = ptr & (a - 1);

  if (modulo != 0) {
    // If 'p' address is not aligned, push the address to the
    // next value which is aligned
    ptr -= modulo;
  }

  return ptr;
}