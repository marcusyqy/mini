#pragma once
#include "defs.hpp"
#include <cassert>

template <typename V, typename T = s32>
struct Relative_Pointer {
private:
  static_assert(std::is_signed_v<T>, "T must be signed");
  enum Value : T {};
  static constexpr auto bit_mask   = 1 << (sizeof(T) * 8 - 1); // make the most high bit 1.
  static constexpr auto Null_Value = Value(0);

  Value offset;

  static T decode(Value v) {
    auto ret = (T)v;
    ret ^= bit_mask;
    return ret;
  }

  static Value encode(intptr_t v) {
    auto ret = (T)v;
    // check for overflow here.
    assert(ret == v);
    v ^= bit_mask;
    return Value(v);
  }

public:
  const T* raw() const { return offset != Null_Value ? &offset + decode(offset) : nullptr; }
  T* raw() { return offset != Null_Value ? &offset + decode(offset) : nullptr; }

  const T* operator->() const { return raw(); }
  T* operator->() { return raw(); }

  T& operator*() {
    auto ptr = raw();
    assert(ptr);
    return *ptr;
  }

  const T& operator*() const {
    auto ptr = raw();
    assert(ptr);
    return *ptr;
  }

  explicit Relative_Pointer(V* o) {
    if (o == nullptr) offset = Null_Value;
    else
      offset = encode(o - &offset);
  }

  template <typename VV, typename TT>
  operator Relative_Pointer<VV, TT>() const {
    return (VV*)raw();
  }
};

enum struct Allocation_Op { 
  alloc, 
  resize, 
  free, 
  alloc_no_zero,
  resize_no_zero 
};

enum struct Allocation_Err {
  none = 0,
  out_of_bounds,    // for realloc & free
  out_of_memory,
};

struct Allocation_Parameters {
  Allocation_Op alloc_instruction;
  void* user_ptr;
  void* memory;
  u64 size;
  u64 old_size;
  u64 alignment;
};

struct Allocation_Result {
  void* memory;
  Allocation_Err info = Allocation_Err::none;
};

void default_allocator_proc(Allocation_Parameters* params, Allocation_Result* result);

struct Allocator {
  using Allocator_Proc      = void (*)(Allocation_Parameters* params, Allocation_Result* result);
  Allocator_Proc alloc_proc = default_allocator_proc;
  void* user_ptr            = nullptr;

  Allocation_Result allocate(u64 size, u64 alignment);
  Allocation_Result allocate_no_zero(u64 size, u64 alignment);
  Allocation_Result realloc(void* memory, u64 size, u64 alignment, u64 old_size);
  Allocation_Result realloc_no_zero(void* memory, u64 size, u64 alignment, u64 old_size);

  Allocation_Err free(void* memory);
};

struct Linear_Allocator_Strategy {
  u8* buf;
  u64 size;
  u64 curr_offset;
  u64 prev_offset;

  void init(u8* _buf, u64 _size);
  Allocation_Result alloc(u64 size, u64 alignment);
  Allocation_Result realloc(void* previous, u64 prev_size, u64 size, u64 alignment);
  void clear();
};

struct Temp_Linear_Allocator;

struct Linear_Allocator {

  template <typename T>
  T* push_no_init() {
    auto p = push(sizeof(T), alignof(T));
    static_assert(std::is_trivially_destructible_v<T>, "Must be defaultly destructible to use no_init");
    return (T*)p;
  }

  template <typename T>
  T* push_array_no_init(u32 N) {
    auto p = push(sizeof(T) * N, alignof(T));
    static_assert(std::is_trivially_destructible_v<T>, "Must be defaultly destructible to use no_init");
    return (T*)p;
  }

  template <typename T>
  T* push_array_zero(u32 N) {
    auto p = push_array_no_init<T>(N);
    memset(p, 0, sizeof(T) * N);
    static_assert(std::is_trivially_destructible_v<T>, "Removing all destructible code.");
    return (T*)p;
  }

  template <typename T>
  T* push_zero() {
    auto p = push(sizeof(T), alignof(T));
    memset(p, 0, sizeof(T));
    return (T*)p;
  }

  void* push(u64 size, u64 alignment);
  void clear();
  void free();

  Linear_Allocator(const Linear_Allocator& o)                = delete;
  Linear_Allocator& operator=(const Linear_Allocator& o)     = delete;
  Linear_Allocator(Linear_Allocator&& o) noexcept            = delete;
  Linear_Allocator& operator=(Linear_Allocator&& o) noexcept = delete;

  ~Linear_Allocator();
  Linear_Allocator(u64 _page_size = mega_bytes(1), Allocator _allocator = {});

private:
  struct Node {
    Node* next = nullptr;
  };

  u8* get_stack_ptr(Node* n) { return (u8*)(n + 1); }

public:
  struct Save_Point {
    Node* current;
    Linear_Allocator* allocator;
    Linear_Allocator_Strategy strategy;
  };

  Temp_Linear_Allocator save();
  void load(Temp_Linear_Allocator save_point);

private:
  Node* head                         = nullptr;
  Node* current                      = nullptr;
  Linear_Allocator_Strategy strategy = {};
  Allocator allocator                = {};
  u64 page_size                      = mega_bytes(1); // default
};

struct Temp_Linear_Allocator {
  template <typename T>
  T* push_no_init() {
    return save_point.allocator->push_no_init<T>();
  }

  template <typename T>
  T* push_array_no_init(u32 N) {
    return save_point.allocator->push_array_no_init<T>(N);
  }

  template <typename T>
  T* push_array_zero(u32 N) {
    return save_point.allocator->push_array_zero<T>(N);
  }

  template <typename T>
  T* push_zero() {
    return save_point.allocator->push_zero<T>();
  }

  void* push(u64 size, u64 alignment) { return save_point.allocator->push(size, alignment); }

  void clear() { save_point.allocator->load(save_point); }

  Temp_Linear_Allocator save() { return save_point.allocator->save(); }

  Temp_Linear_Allocator(Linear_Allocator& allocator) : Temp_Linear_Allocator(allocator.save()) {}
  Temp_Linear_Allocator(Linear_Allocator::Save_Point _save_point) : save_point(_save_point) {}

  // these suck right now.
  Linear_Allocator::Save_Point save_point;
};
