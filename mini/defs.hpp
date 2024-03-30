#pragma once
#include <cstdint>
#include <cstdlib>

#define mini_is_power_of_two(X) (((X) & ((X)-1)) == 0)

using f32 = float;
using f64 = double;

// ensuring this is correct. Or we should find an alternative
static_assert(sizeof(f32) == 4);
static_assert(sizeof(f64) == 8);

// ints
using s8 = int8_t;
using u8 = uint8_t;

using s16 = int16_t;
using u16 = uint16_t;

using s32 = int32_t;
using u32 = uint32_t;

using s64 = int64_t;
using u64 = uint64_t;

// NOTE: let's evaluate if this is worth it.
using cstr = const char*;