// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_IMMUTABLE_VECTOR_H_
#define TOOLS_GN_IMMUTABLE_VECTOR_H_

#include <cstdlib>
#include <type_traits>
#include <utility>

#include "util/aligned_alloc.h"

// An ImmutableVector<T> represents a fixed-size vector of constant items of
// type T. The in-memory representation is more efficient, only using one
// pointer to a single heap-allocated memory block that also contains the size.
//
// An ImmutableVectorView<T> acts as a copyable and movable reference to another
// ImmutableVector<T> instance. They both point to the same memory in the heap,
// but the view is not owning and is invalidated when the instance it points to
// is destroyed.
//
// Apart from that, they can be used with the same methods as a  const
// std::vector<T>.
//
template <typename T>
class ImmutableVector;

template <typename T>
class ImmutableVectorView {
 public:
  // Default constructor
  ImmutableVectorView() = default;

  // Constructor from an ImmutableVector.
  ImmutableVectorView(const ImmutableVector<T>& vector)
      : header_(vector.header_) {}

  // Usual methods to access items.
  const T* data() const { return begin(); }
  size_t size() const { return header_ ? header_->size : 0u; }
  bool empty() const { return size() == 0; }
  const T& operator[](size_t offset) const { return begin()[offset]; };

  const T* begin() const { return header_ ? &header_->item0 : nullptr; }
  const T* end() const {
    return header_ ? &header_->item0 + header_->size : nullptr;
  }

  const T& front() const { return begin()[0]; }
  const T& back() const { return end()[-1]; }

  // For use with standard algorithms and templates.
  using element_type = T;
  using value_type = std::remove_cv_t<T>;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using iterator = const T*;
  using const_iterator = const T*;

  const_iterator find(const T& item) const {
    auto it = begin();
    auto it_end = end();
    for (; it != it_end; ++it) {
      if (*it == item)
        break;
    }
    return it;
  }

  bool contains(const T& item) const {
    for (const auto& cur_item : *this)
      if (cur_item == item)
        return true;

    return false;
  }

 protected:
  struct Header {
    size_t size;
    T item0;
  };

  ImmutableVectorView(Header* header) : header_(header) {}

  Header* header_ = nullptr;
};

template <typename T>
class ImmutableVector : public ImmutableVectorView<T> {
 public:
  // Default constructor.
  ImmutableVector() = default;

  // Range constructors.
  ImmutableVector(const T* begin, size_t size)
      : ImmutableVectorView<T>(AllocateAndCopyFrom(begin, size)) {}

  ImmutableVector(const T* begin, const T* end)
      : ImmutableVector(begin, end - begin) {}

  // In-place constructor
  // |producer| must be a callable that generates a T instance that will
  // be used to construct items in place in the allocated vector.
  template <typename P,
            typename = std::void_t<
                decltype(static_cast<const T&>(std::declval<P>()()))>>
  ImmutableVector(P producer, size_t size) {
    if (size) {
      this->header_ = AllocateFor(size);
      auto* dst = &(this->header_->item0);
      auto* dst_limit = dst + size;
      for (; dst != dst_limit; ++dst)
        new (dst) T(producer());
    }
  }

  // Container constructor
  //
  // This uses std::void_t<> to select container types whose values
  // are convertible to |const T&|, and which have |begin()| and |size()|
  // methods.
  //
  // This constructor copies items from the constructor into the
  // ImmutableVector.
  template <typename C,
            typename = std::void_t<
                decltype(static_cast<const T>(*std::declval<C>().begin())),
                decltype(std::declval<C>().size())>>
  ImmutableVector(const C& container) {
    size_t size = container.size();
    if (size) {
      this->header_ = AllocateFor(size);
      auto src = container.begin();
      auto* dst = &(this->header_->item0);
      auto* dst_limit = dst + size;
      for (; dst != dst_limit; ++dst, ++src) {
        new (dst) T(*src);
      }
    }
  }

  // Another container constructor where the items can be moved into
  // the resulting ImmutableVector.
  template <typename C,
            typename = std::void_t<
                decltype(static_cast<const T>(*std::declval<C>().begin())),
                decltype(std::declval<C>().size())>>
  ImmutableVector(C&& container) {
    size_t size = container.size();
    if (size) {
      this->header_ = AllocateFor(size);
      auto src = container.begin();
      auto* dst = &(this->header_->item0);
      auto* dst_limit = dst + size;
      for (; dst != dst_limit; ++dst, ++src)
        new (dst) T(std::move(*src));
    }
  }

  // Initializer-list container.
  ImmutableVector(std::initializer_list<T> list)
      : ImmutableVector(list.begin(), list.size()) {}

  // Copy operations
  ImmutableVector(const ImmutableVector& other)
      : ImmutableVectorView<T>(
            AllocateAndCopyFrom(other.begin(), other.size())) {}

  ImmutableVector& operator=(const ImmutableVector& other) {
    if (this != &other) {
      this->~ImmutableVector();
      new (this) ImmutableVector(other);
    }
    return *this;
  }

  // Move operations
  ImmutableVector(ImmutableVector&& other) noexcept {
    this->header_ = other.header_;
    other.header_ = nullptr;
  }

  ImmutableVector& operator=(ImmutableVector&& other) noexcept {
    if (this != &other) {
      this->~ImmutableVector();
      new (this) ImmutableVector(std::move(other));
    }
    return *this;
  }

  // Destructor
  ~ImmutableVector() {
    if (this->header_) {
      auto* cur = &(this->header_->item0);
      auto* limit = cur + this->size();
      while (cur != limit) {
        (*cur).~T();
        cur++;
      }
      Allocator::Free(this->header_);
    }
  }

 protected:
  friend class ImmutableVectorView<T>;

  using Header = typename ImmutableVectorView<T>::Header;

  // Don't use std::max() here to avoid including <algorithm> which is massive.
  static constexpr size_t kAlignment = alignof(T) > alignof(Header)
                                           ? alignof(T)
                                           : alignof(Header);

  using Allocator = AlignedAlloc<kAlignment>;

  static Header* AllocateFor(size_t count) {
    if (!count)
      return nullptr;

    auto* header = reinterpret_cast<Header*>(
        Allocator::Alloc(sizeof(Header) + (count - 1u) * sizeof(T)));
    header->size = count;
    return header;
  }

  static Header* AllocateAndCopyFrom(const T* begin, size_t size) {
    Header* header = AllocateFor(size);
    if (size) {
      T* dst = &(header->item0);
      T* limit = dst + size;
      for (; dst != limit; ++dst, ++begin)
        new (dst) T(*begin);
    }
    return header;
  }
};

#endif  // TOOLS_GN_IMMUTABLE_VECTOR_H_
