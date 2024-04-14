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


