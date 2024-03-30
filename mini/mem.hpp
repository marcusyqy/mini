#pragma once
#include "defs.hpp"
#include <cassert>
#include <memory>

namespace detail {

struct Destructor_Node {
  void* ptr;
  void (*destruct)(void*);
  Destructor_Node* next;
};

} // namespace detail

template <u64 Stack_Size>
struct Stack_Allocator {
  static_assert(mini_is_power_of_two(Stack_Size), "Stack pages must be in power of two.");
  static constexpr u64 page_size = Stack_Size;

  // this needs to be specific for T
  u8* push(u64 size, u64 alignment) {
    assert(mini_is_power_of_two(alignment));
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
        n->stack.current = (u64)result;
        return (u8*)p;
      }
      node = &n->next;
    }

    *node = new Stack_Node;
    assert(*node);
    auto& n  = *node;
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
    n->stack.current = (u64)result;
    return (u8*)p;
  }

  template <typename T, typename... Args>
  T* push(Args&&... args) {
    auto p   = push(sizeof(T), alignof(T));
    auto ret = ::new (p) T{ std::forward<Args&&>(args)... };
    if constexpr (!std::is_trivially_destructible_v<T>) {
      auto node = (detail::Destructor_Node*)push(sizeof(detail::Destructor_Node), alignof(detail::Destructor_Node));
      node->ptr = p;
      node->destruct  = +[](void* ptr) { std::destroy_at((T*)ptr); };
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
      destructor_list->destruct(destructor_list->ptr);
      destructor_list = destructor_list->next;
    }
  }

  void free() {
    while (destructor_list) {
      destructor_list->destruct(destructor_list->ptr);
      destructor_list = destructor_list->next;
    }

    while (head) {
      auto tmp = head->next;
      delete tmp;
      head = tmp;
    }
  }

  Stack_Allocator()                                        = default;
  Stack_Allocator(const Stack_Allocator& o)                = delete;
  Stack_Allocator& operator=(const Stack_Allocator& o)     = delete;
  Stack_Allocator(Stack_Allocator&& o) noexcept            = delete;
  Stack_Allocator& operator=(Stack_Allocator&& o) noexcept = delete;

  ~Stack_Allocator() { free(); }

private:
  struct Stack {
    u8 buffer[Stack_Size];
    u64 current = {};
  };

  struct Stack_Node {
    Stack stack;
    Stack_Node* next = nullptr;
  };

private:
  Stack_Node* head                         = nullptr;
  detail::Destructor_Node* destructor_list = nullptr;
};

using Default_Stack_Allocator = Stack_Allocator<1u << 12>;
