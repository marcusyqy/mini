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

struct Allocator_Proc {
  void* (*_allocate)(void* allocator, u32 size, u32 alignment) = +[](void*, u32 size, u32 alignment) -> void* {
    return ::malloc(size);
  };
  void (*_free)(void* allocator, void* memory) = +[](void*, void* memory) { ::free(memory); };
  void* _allocator                             = nullptr;

  void* allocate(u32 size, u32 alignment) {
    assert(_allocate);
    return _allocate(_allocator, size, alignment);
  }

  void free(void* memory) {
    assert(_free);
    _free(_allocator, memory);
  }
};

template <u32 Stack_Size>
struct Stack_Allocator {
  static_assert(is_power_of_two(Stack_Size), "Stack pages must be in power of two.");
  static constexpr u32 page_size = Stack_Size;

  // this needs to be specific for T
  u8* push(u32 size, u32 alignment) {
    assert(is_power_of_two(alignment));
    Stack_Node** node = &head;

    // go through all the pages.
    while (*node) {
      auto& n  = *node;
      auto buf = (uintptr_t)n->stack.buffer;
      auto p   = buf + (uintptr_t)n->stack.current;
      auto a   = (uintptr_t)alignment;
      auto mod = p & (a - 1);
      if (mod != 0) {
        p += a - mod;
      }
      auto result = p + (uintptr_t)size - buf;
      if (result < page_size) {
        assert(result >= 0);
        n->stack.current = (u32)result;
        return (u8*)p;
      }
      node = &n->next;
    }

    *node = (Stack_Node*)alloc_proc.allocate(sizeof(Stack_Node), alignof(Stack_Node));
    assert(*node);
    auto& n = *node;
    // should i consider memsetting?
    n->stack.current = 0; // need to reset in the event that allocate doesn't memset.
    n->next          = nullptr;

    auto buf = (uintptr_t)n->stack.buffer;
    auto p   = buf + (uintptr_t)n->stack.current;
    auto a   = (uintptr_t)alignment;
    auto mod = p & (a - 1);
    if (mod != 0) {
      p += a - mod;
    }
    auto result = p + (uintptr_t)size - buf;
    assert(result < page_size);
    assert(result >= 0);
    n->stack.current = (u32)result;
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
      tmp->stack.current = 0;
      tmp                = tmp->next;
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

  Stack_Allocator()                                        = default;
  Stack_Allocator(const Stack_Allocator& o)                = delete;
  Stack_Allocator& operator=(const Stack_Allocator& o)     = delete;
  Stack_Allocator(Stack_Allocator&& o) noexcept            = delete;
  Stack_Allocator& operator=(Stack_Allocator&& o) noexcept = delete;

  ~Stack_Allocator() { free(); }

  Stack_Allocator(Allocator_Proc _alloc_proc) : alloc_proc(_alloc_proc) {}

private:
  struct Stack {
    u8 buffer[Stack_Size];
    u32 current = {};
  };

  struct Stack_Node {
    Stack stack;
    Stack_Node* next = nullptr;
  };

private:
  Stack_Node* head                         = nullptr;
  detail::Destructor_Node* destructor_list = nullptr;
  Allocator_Proc alloc_proc                = {};
};

using Default_Stack_Allocator = Stack_Allocator<1u << 16>;
