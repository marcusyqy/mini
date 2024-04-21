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

enum struct Allocation_Instruction { alloc, resize, free, alloc_no_zero };

enum struct Allocation_Info {
  none = 0,
  relocated_memory, // for realloc
  out_of_bounds,    // for realloc & free
  out_of_memory,
};

struct Allocation_Parameters {
  Allocation_Instruction alloc_instruction;
  void* user_ptr;
  void* memory;
  u64 size;
  u64 alignment;
};

struct Allocation_Return {
  void* memory;
  Allocation_Info result;
};

Allocation_Return default_allocator_proc(Allocation_Parameters params);

struct Allocator {
  using Allocator_Proc      = Allocation_Return (*)(Allocation_Parameters params);
  Allocator_Proc alloc_proc = default_allocator_proc;
  void* user_ptr            = nullptr;

  Allocation_Return allocate(u64 size, u64 alignment);
  Allocation_Return allocate_no_zero(u32 size, u32 alignment);
  Allocation_Info free(void* memory);
  Allocation_Return realloc(void* memory, u32 size, u32 alignment);
};

struct Linear_Allocator_Strategy {
  u8* buf;
  u64 size;
  u64 curr_offset;
  u64 prev_offset;

  void init(u8* _buf, u64 _size);
  Allocation_Return alloc(u64 size, u64 alignment);
  Allocation_Return realloc(void* previous, u64 prev_size, u64 size, u64 alignment);
  void clear();
};

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

  Linear_Allocator() = default;
  Linear_Allocator(u64 _page_size, Allocator _allocator = {});

private:
  struct Node {
    Node* next = nullptr;
  };

  u8* get_stack_ptr(Node* n) { return (u8*)(n + 1); }

  struct Save_Point {
    Node* current;
    const Linear_Allocator* whoami;
    Linear_Allocator_Strategy strategy;
  };

public:
  Save_Point save_current() const;
  void load(Save_Point save_point);

 private:
  Node* head                         = nullptr;
  Node* current                      = nullptr;
  Linear_Allocator_Strategy strategy = {};
  Allocator allocator                = {};
  u64 page_size                      = mega_bytes(1); // default
};
