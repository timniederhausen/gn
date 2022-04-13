// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_TARGET_PUBLIC_PAIR_H_
#define TOOLS_GN_TARGET_PUBLIC_PAIR_H_

#include "gn/immutable_vector.h"
#include "gn/tagged_pointer.h"
#include "gn/unique_vector.h"

class Target;

// A Compact encoding for a (target_ptr, is_public_flag) pair.
class TargetPublicPair {
 public:
  TargetPublicPair() = default;
  TargetPublicPair(const Target* target, bool is_public)
      : pair_(target, static_cast<unsigned>(is_public)) {}
  TargetPublicPair(std::pair<const Target*, bool> pair)
      : pair_(pair.first, static_cast<unsigned>(pair.second)) {}

  const Target* target() const { return pair_.ptr(); }
  void set_target(const Target* target) { pair_.set_ptr(target); }

  bool is_public() const { return pair_.tag() != 0; }
  void set_is_public(bool is_public) { pair_.set_tag(is_public ? 1 : 0); }

  // Utility structs that can be used to instantiante containers
  // that only use the target for lookups / comparisons. E.g.
  //
  //   std::unordered_set<TargetPublicPair,
  //                      TargetPublicPair::TargetHash,
  //                      TargetPublicPair::TargetEqualTo>
  //
  //   std::set<TargetPublicPair, TargetPublicPair::TargetLess>
  //
  struct TargetHash {
    size_t operator()(TargetPublicPair p) const noexcept {
      return std::hash<const Target*>()(p.target());
    }
  };

  struct TargetEqualTo {
    bool operator()(TargetPublicPair a, TargetPublicPair b) const noexcept {
      return a.target() == b.target();
    }
  };

  struct TargetLess {
    bool operator()(TargetPublicPair a, TargetPublicPair b) const noexcept {
      return a.target() < b.target();
    }
  };

 private:
  TaggedPointer<const Target, 1> pair_;
};

// A helper type to build a list of (target, is_public) pairs, where target
// pointers are unique. Usage is:
//
//   1) Create builder instance.
//   2) Call Append() or AppendInherited() as many times as necessary.
//   3) Call Build() to retrieve final list as an immutable vector.
//
class TargetPublicPairListBuilder
    : public UniqueVector<TargetPublicPair,
                          TargetPublicPair::TargetHash,
                          TargetPublicPair::TargetEqualTo> {
 public:
  // Add (target, is_public) to the list being constructed. If the target
  // was not already in the list, recorded the |is_public| flag as is,
  // otherwise, set the recorded flag to true only if |is_public| is true, or
  // don't do anything otherwise.
  void Append(const Target* target, bool is_public) {
    auto ret = EmplaceBackWithIndex(target, is_public);
    if (!ret.first && is_public) {
      // UniqueVector<T>::operator[]() always returns a const reference
      // because the returned values are lookup keys in its set-like data
      // structure (thus modifying them would break its internal consistency).
      // However, because TargetHash and TargetEqualTo are being used to
      // instantiate this template, only the target() part of the value must
      // remain constant, and it is possible to modify the is_public() part
      // in-place safely.
      auto* pair = const_cast<TargetPublicPair*>(&(*this)[ret.second]);
      pair->set_is_public(true);
    }
  }

  // Append all pairs from any container with begin() and end() iterators
  // that dereference to values that convert to a TargetPublicPair value.
  // If |is_public| is false, the input pair will be appended with the
  // value of the public flag to false.
  template <
      typename C,
      typename = std::void_t<
          decltype(static_cast<TargetPublicPair>(*std::declval<C>().begin())),
          decltype(static_cast<TargetPublicPair>(*std::declval<C>().end()))>>
  void AppendInherited(const C& other, bool is_public) {
    for (const auto& pair : other) {
      Append(pair.target(), is_public && pair.is_public());
    }
  }

  ImmutableVector<TargetPublicPair> Build() {
    return ImmutableVector<TargetPublicPair>(release());
  }
};

#endif  // TOOLS_GN_TARGET_PUBLIC_PAIR_H_
