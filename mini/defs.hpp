#pragma once
#include <cstdint>
#include <utility>

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


namespace detail {
template <typename Fn>
struct Defer {
    Defer(Fn&& f) : fn(std::move(f)) {}
    ~Defer() { fn(); }

private:
    Fn fn;
};

#define CONCATENATE_IMPL(s1, s2) s1##s2
#define CONCATENATE(s1, s2)      CONCATENATE_IMPL(s1, s2)

#ifdef __COUNTER__
#define ANONYMOUS_VARIABLE(str) CONCATENATE(str, __COUNTER__)
#else
#define ANONYMOUS_VARIABLE(str) CONCATENATE(str, __LINE__)
#endif

enum class DeferHelper {};

template <typename Fn>
Defer<Fn> operator+(DeferHelper, Fn&& fn) {
    return Defer<Fn>{ std::forward<Fn&&>(fn) };
}

} // namespace detail

#define defer              auto ANONYMOUS_VARIABLE(DEFER_FUNCTION) = ::detail::DeferHelper() + [&]()
#define DEFER              defer

#define kilo_bytes(bytes) ((bytes) << 10) 
#define mega_bytes(bytes) ((bytes) << 20) 
#define giga_bytes(bytes) ((bytes) << 30) 

#define is_power_of_two(X) (((X) & ((X)-1)) == 0)