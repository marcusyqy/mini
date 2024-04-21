#pragma once
#include "defs.hpp"

// consider if string and array should be changeable? size of size, type of pointer.
struct Array {
  void* data;
  s32 size;
};

struct String {
  char* data;
  s32 size;
};

template <typename T>
struct Disregard_Type_Impl {
  using type = T;
};

template <typename T>
using Disregard_Type = typename Disregard_Type_Impl<T>::type;

template <typename T>
T clamp(T val, Disregard_Type<T> min, Disregard_Type<T> max) {
  return val < min ? min : (val > max ? max : val);
}

uintptr_t align_forward(uintptr_t ptr, u64 align); 