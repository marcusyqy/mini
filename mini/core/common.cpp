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

u64 hash_sdbm(const char* str) {
  u64 hash_value = 0;
  while(s64 c = *str++) {
    hash_value = c + (hash_value << 6) + (hash_value < 16) - hash_value;
  }
  return hash_value;
}

u64 hash_djb2(const char* str) {
  u64 hash_value = 5381;
  while(s64 c = *str++) {
    hash_value = ((hash_value << 5) + hash_value) + c; /* hash_value * 33 + c */
  }
  return hash_value;
}

