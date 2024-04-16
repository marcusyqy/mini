#pragma once
#include "defs.hpp"
#include <cassert>
#include <memory>

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

// template<typename T>
// struct Pointer : Relative_Pointer<T> {

// };

// template<>
// struct Pointer<void> { // normal pointer to allow some templatey stuff?

// };

namespace detail {

struct Destructor_Node {
  void* ptr;
  void (*destruct)(void*, u32 size);
  u32 size;
  Destructor_Node* next;
};

} // namespace detail

enum struct Allocation_Instruction { alloc, resize, free };

struct Allocator_Proc {
  using _Alloc_Proc =
      void* (*)(Allocation_Instruction alloc_instruction, void* allocator, void* memory, u32 size, u32 alignment);

  _Alloc_Proc _alloc_proc =
      +[](Allocation_Instruction alloc_instruction, void* allocator, void* memory, u32 size, u32 alignment) -> void* {
    switch (alloc_instruction) {
      case Allocation_Instruction::alloc: return ::malloc(size);
      case Allocation_Instruction::free: ::free(memory); return nullptr;
      case Allocation_Instruction::resize: return ::realloc(memory, size);
    }
    assert(false);
    return nullptr;
  };
  void* _allocator = nullptr;

  // TODO: change this to allocator instead of allocator proc
  void* allocate(u32 size, u32 alignment) {
    assert(_alloc_proc);
    return _alloc_proc(Allocation_Instruction::alloc, _allocator, nullptr, size, alignment);
  }

  void free(void* memory) {
    assert(_alloc_proc);
    _alloc_proc(Allocation_Instruction::free, _allocator, memory, 0, 0);
  }

  void* realloc(void* memory, u32 size, u32 alignment) {
    assert(_alloc_proc);
    _alloc_proc(Allocation_Instruction::free, _allocator, memory, size, alignment);
  }
};

struct Linear_Allocator {
  // this needs to be specific for T
  u8* push(u32 size, u32 alignment) {
    assert(is_power_of_two(alignment));
    Node** node = &head;

    // go through all the pages.
    while (*node) {
      auto& n  = *node;
      auto buf = (uintptr_t)get_stack_ptr(n);
      auto p   = buf + (uintptr_t)n->current;
      auto a   = (uintptr_t)alignment;
      auto mod = p & (a - 1);
      if (mod != 0) {
        p += a - mod;
      }
      auto result = p + (uintptr_t)size - buf;
      if (result < page_size) {
        assert(result >= 0);
        n->current = (u32)result;
        return (u8*)p;
      }
      node = &n->next;
    }

    *node = (Node*)alloc_proc.allocate(sizeof(Node) + page_size, alignof(Node));
    assert(*node);

    auto& n = *node;
    memset(n, 0, sizeof(Node)); // doesn't memset the bytes.

    auto buf = (uintptr_t)get_stack_ptr(n);
    auto p   = buf + (uintptr_t)n->current;
    auto a   = (uintptr_t)alignment;
    auto mod = p & (a - 1);
    if (mod != 0) p += a - mod;

    auto result = p + (uintptr_t)size - buf;
    assert(result < page_size);
    assert(result >= 0);
    n->current = (u32)result;
    return (u8*)p;
  }

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

  template <typename T, typename... Args>
  T* push_array(u32 N, Args&&... args) {
    auto p   = push(sizeof(T) * N, alignof(T));
    auto ret = ::new (p) T{ std::forward<Args&&>(args)... };
    if constexpr (!std::is_trivially_destructible_v<T>) {
      auto node = (detail::Destructor_Node*)push(sizeof(detail::Destructor_Node), alignof(detail::Destructor_Node));
      node->ptr = p;
      node->destruct  = +[](void* ptr, u32 size) { std::destroy_n((T*)ptr, size); };
      node->size      = N;
      node->next      = destructor_list;
      destructor_list = node;
    }
    return ret;
  }

  template <typename T, typename... Args>
  T* push(Args&&... args) {
    auto p   = push(sizeof(T), alignof(T));
    auto ret = ::new (p) T{ std::forward<Args&&>(args)... };
    if constexpr (!std::is_trivially_destructible_v<T>) {
      auto node = (detail::Destructor_Node*)push(sizeof(detail::Destructor_Node), alignof(detail::Destructor_Node));
      node->ptr = p;
      node->destruct  = +[](void* ptr, u32) { std::destroy_at((T*)ptr); };
      node->size      = 0;
      node->next      = destructor_list;
      destructor_list = node;
    }
    return ret;
  }

  // need to call destructor for some T
  void clear() {
    auto tmp = head;

    while (tmp) {
      tmp->current = 0;
      tmp          = tmp->next;
    }

    while (destructor_list) {
      destructor_list->destruct(destructor_list->ptr, destructor_list->size);
      destructor_list = destructor_list->next;
    }
  }

  void free() {
    while (destructor_list) {
      destructor_list->destruct(destructor_list->ptr, destructor_list->size);
      destructor_list = destructor_list->next;
    }

    while (head) {
      auto tmp = head->next;
      alloc_proc.free((void*)tmp);
      head = tmp;
    }
  }

  Linear_Allocator(const Linear_Allocator& o)                = delete;
  Linear_Allocator& operator=(const Linear_Allocator& o)     = delete;
  Linear_Allocator(Linear_Allocator&& o) noexcept            = delete;
  Linear_Allocator& operator=(Linear_Allocator&& o) noexcept = delete;

  ~Linear_Allocator() { free(); }

  Linear_Allocator() = default;
  Linear_Allocator(u32 _page_size, Allocator_Proc _alloc_proc = {}) :
      page_size{ _page_size }, alloc_proc{ _alloc_proc } {
    assert(is_power_of_two(page_size));
  }

private:
  struct Node {
    Node* next  = nullptr;
    u32 current = {}; // one way we can do it here is to make current be shared across
    // wasting 4 bytes of stuff here.
  };

  Node* get_stack_ptr(Node* n) { return n + 1; }

private:
  Node* head                               = nullptr;
  detail::Destructor_Node* destructor_list = nullptr;
  Allocator_Proc alloc_proc                = {};
  u32 page_size                            = mega_bytes(1); // default
};
