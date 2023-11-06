// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_MANAGED_BYTE_STRING_H_
#define THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_MANAGED_BYTE_STRING_H_

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/casts.h"
#include "absl/log/absl_check.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/internal/reference_count.h"
#include "internal/overloaded.h"

namespace cel::common_internal {

#ifdef _MSC_VER
#pragma pack(pack, 1)
#endif

struct ABSL_ATTRIBUTE_PACKED ManagedByteStringHeader final {
  // True if the content is `absl::Cord`.
  bool is_cord : 1;
  // Only used when `is_cord` is `false`.
  size_t size : sizeof(size_t) * 8 - 1;

  ManagedByteStringHeader(bool is_cord, size_t size)
      : is_cord(is_cord), size(size) {
    // Ensure size does not occupy the most significant bit.
    ABSL_DCHECK_GE(absl::bit_cast<std::make_signed_t<size_t>>(size), 0);
  }
};

#ifdef _MSC_VER
#pragma pack(pop)
#endif

static_assert(sizeof(ManagedByteStringHeader) == sizeof(size_t));

class ManagedByteString;
class ABSL_ATTRIBUTE_TRIVIAL_ABI ManagedByteStringView;

// `ManagedByteString` is a compact wrapper around either an `absl::Cord` or
// `absl::string_view` with `const ReferenceCount*`.
class ManagedByteString final {
 public:
  ManagedByteString() noexcept : ManagedByteString(absl::string_view()) {}

  explicit ManagedByteString(absl::string_view string_view) noexcept
      : ManagedByteString(nullptr, string_view) {}

  explicit ManagedByteString(
      const std::string& string ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : ManagedByteString(absl::string_view(string)) {}

  explicit ManagedByteString(std::string&& string)
      : ManagedByteString(absl::Cord(std::move(string))) {}

  // Constructs a `ManagedByteString` whose contents are `string_view` owned by
  // `refcount`. If `refcount` is not nullptr, a strong reference is taken.
  ManagedByteString(const ReferenceCount* refcount,
                    absl::string_view string_view) noexcept
      : header_(false, string_view.size()) {
    content_.string.data = string_view.data();
    content_.string.refcount = refcount;
    (StrongRef)(refcount);
  }

  explicit ManagedByteString(absl::Cord cord) noexcept : header_(true, 0) {
    ::new (static_cast<void*>(cord_ptr())) absl::Cord(std::move(cord));
  }

  explicit ManagedByteString(ManagedByteStringView other) noexcept;

  ManagedByteString(const ManagedByteString& other) noexcept
      : header_(other.header_) {
    if (header_.is_cord) {
      ::new (static_cast<void*>(cord_ptr())) absl::Cord(*other.cord_ptr());
    } else {
      content_.string.data = other.content_.string.data;
      content_.string.refcount = other.content_.string.refcount;
      (StrongRef)(content_.string.refcount);
    }
  }

  ManagedByteString(ManagedByteString&& other) noexcept
      : header_(other.header_) {
    if (header_.is_cord) {
      ::new (static_cast<void*>(cord_ptr()))
          absl::Cord(std::move(*other.cord_ptr()));
    } else {
      content_.string.data = other.content_.string.data;
      content_.string.refcount = other.content_.string.refcount;
      other.content_.string.data = "";
      other.content_.string.refcount = nullptr;
      other.header_.size = 0;
    }
  }

  ~ManagedByteString() noexcept {
    if (header_.is_cord) {
      cord_ptr()->~Cord();
    } else {
      (StrongUnref)(content_.string.refcount);
    }
  }

  ManagedByteString& operator=(const ManagedByteString& other) noexcept {
    this->~ManagedByteString();
    ::new (static_cast<void*>(this)) ManagedByteString(other);
    return *this;
  }

  ManagedByteString& operator=(ManagedByteString&& other) noexcept {
    this->~ManagedByteString();
    ::new (static_cast<void*>(this)) ManagedByteString(std::move(other));
    return *this;
  }

  template <typename Visitor>
  std::common_type_t<std::invoke_result_t<Visitor, absl::string_view>,
                     std::invoke_result_t<Visitor, const absl::Cord&>>
  Visit(Visitor&& visitor) const {
    if (header_.is_cord) {
      return std::forward<Visitor>(visitor)(*cord_ptr());
    } else {
      return std::forward<Visitor>(visitor)(
          absl::string_view(content_.string.data, header_.size));
    }
  }

  void swap(ManagedByteString& other) noexcept {
    using std::swap;
    if (header_.is_cord) {
      // absl::Cord
      if (other.header_.is_cord) {
        // absl::Cord
        swap(*cord_ptr(), *other.cord_ptr());
      } else {
        // absl::string_view
        SwapMixed(*this, other);
      }
    } else {
      // absl::string_view
      if (other.header_.is_cord) {
        // absl::Cord
        SwapMixed(other, *this);
      } else {
        // absl::string_view
        swap(content_.string.data, other.content_.string.data);
        swap(content_.string.refcount, other.content_.string.refcount);
      }
    }
    swap(header_, other.header_);
  }

  // Retrieves the contents of this byte string as `absl::string_view`. If this
  // byte string is backed by an `absl::Cord` which is not flat, `scratch` is
  // used to store the contents and the returned `absl::string_view` is a view
  // of `scratch`.
  absl::string_view ToString(std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND)
      const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return Visit(internal::Overloaded{
        [](absl::string_view string) -> absl::string_view { return string; },
        [&scratch](const absl::Cord& cord) -> absl::string_view {
          if (auto flat = cord.TryFlat(); flat.has_value()) {
            return *flat;
          }
          scratch = static_cast<std::string>(cord);
          return absl::string_view(scratch);
        }});
  }

  std::string ToString() const {
    return Visit(
        internal::Overloaded{[](absl::string_view string) -> std::string {
                               return std::string(string);
                             },
                             [](const absl::Cord& cord) -> std::string {
                               return static_cast<std::string>(cord);
                             }});
  }

  absl::Cord ToCord() const {
    return Visit(internal::Overloaded{
        [this](absl::string_view string) -> absl::Cord {
          const auto* refcount = content_.string.refcount;
          if (refcount != nullptr) {
            (StrongRef)(*refcount);
            return absl::MakeCordFromExternal(
                string, [refcount]() { (StrongUnref)(*refcount); });
          }
          return absl::Cord(string);
        },
        [](const absl::Cord& cord) -> absl::Cord { return cord; }});
  }

 private:
  friend class ManagedByteStringView;

  static void SwapMixed(ManagedByteString& cord,
                        ManagedByteString& string) noexcept {
    const auto* string_data = string.content_.string.data;
    const auto* string_refcount = string.content_.string.refcount;
    ::new (static_cast<void*>(string.cord_ptr()))
        absl::Cord(std::move(*cord.cord_ptr()));
    cord.cord_ptr()->~Cord();
    cord.content_.string.data = string_data;
    cord.content_.string.refcount = string_refcount;
  }

  absl::Cord* cord_ptr() noexcept {
    return reinterpret_cast<absl::Cord*>(&content_.cord[0]);
  }

  const absl::Cord* cord_ptr() const noexcept {
    return reinterpret_cast<const absl::Cord*>(&content_.cord[0]);
  }

  ManagedByteStringHeader header_;
  union {
    struct {
      const char* data;
      const ReferenceCount* refcount;
    } string;
    alignas(absl::Cord) char cord[sizeof(absl::Cord)];
  } content_;
};

inline void swap(ManagedByteString& lhs, ManagedByteString& rhs) noexcept {
  lhs.swap(rhs);
}

class ABSL_ATTRIBUTE_TRIVIAL_ABI ManagedByteStringView final {
 public:
  ManagedByteStringView() noexcept
      : ManagedByteStringView(absl::string_view()) {}

  explicit ManagedByteStringView(absl::string_view string) noexcept
      : ManagedByteStringView(nullptr, string) {}

  explicit ManagedByteStringView(
      const std::string& string ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : ManagedByteStringView(absl::string_view(string)) {}

  ManagedByteStringView(const ReferenceCount* refcount,
                        absl::string_view string) noexcept
      : header_(false, string.size()) {
    content_.string.data = string.data();
    content_.string.refcount = refcount;
  }

  explicit ManagedByteStringView(
      const absl::Cord& cord ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : header_(true, 0) {
    content_.cord = &cord;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  ManagedByteStringView(
      const ManagedByteString& other ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : header_(other.header_) {
    if (header_.is_cord) {
      content_.cord = other.cord_ptr();
    } else {
      content_.string.data = other.content_.string.data;
      content_.string.refcount = other.content_.string.refcount;
    }
  }

  ManagedByteStringView(const ManagedByteStringView&) = default;
  ManagedByteStringView& operator=(const ManagedByteStringView&) = default;

  template <typename Visitor>
  std::common_type_t<std::invoke_result_t<Visitor, absl::string_view>,
                     std::invoke_result_t<Visitor, const absl::Cord&>>
  Visit(Visitor&& visitor) const {
    if (header_.is_cord) {
      return std::forward<Visitor>(visitor)(*content_.cord);
    } else {
      return std::forward<Visitor>(visitor)(
          absl::string_view(content_.string.data, header_.size));
    }
  }

  void swap(ManagedByteStringView& other) noexcept {
    using std::swap;
    swap(header_, other.header_);
    swap(content_, other.content_);
  }

  // Retrieves the contents of this byte string as `absl::string_view`. If this
  // byte string is backed by an `absl::Cord` which is not flat, `scratch` is
  // used to store the contents and the returned `absl::string_view` is a view
  // of `scratch`.
  absl::string_view ToString(std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND)
      const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return Visit(internal::Overloaded{
        [](absl::string_view string) -> absl::string_view { return string; },
        [&scratch](const absl::Cord& cord) -> absl::string_view {
          if (auto flat = cord.TryFlat(); flat.has_value()) {
            return *flat;
          }
          scratch = static_cast<std::string>(cord);
          return absl::string_view(scratch);
        }});
  }

  std::string ToString() const {
    return Visit(
        internal::Overloaded{[](absl::string_view string) -> std::string {
                               return std::string(string);
                             },
                             [](const absl::Cord& cord) -> std::string {
                               return static_cast<std::string>(cord);
                             }});
  }

  absl::Cord ToCord() const {
    return Visit(internal::Overloaded{
        [this](absl::string_view string) -> absl::Cord {
          const auto* refcount = content_.string.refcount;
          if (refcount != nullptr) {
            (StrongRef)(*refcount);
            return absl::MakeCordFromExternal(
                string, [refcount]() { (StrongUnref)(*refcount); });
          }
          return absl::Cord(string);
        },
        [](const absl::Cord& cord) -> absl::Cord { return cord; }});
  }

 private:
  friend class ManagedByteString;

  ManagedByteStringHeader header_;
  union {
    struct {
      const char* data;
      const ReferenceCount* refcount;
    } string;
    const absl::Cord* cord;
  } content_;
};

inline ManagedByteString::ManagedByteString(
    ManagedByteStringView other) noexcept
    : header_(other.header_) {
  if (header_.is_cord) {
    ::new (static_cast<void*>(cord_ptr())) absl::Cord(*other.content_.cord);
  } else {
    content_.string.data = other.content_.string.data;
    content_.string.refcount = other.content_.string.refcount;
    (StrongRef)(content_.string.refcount);
  }
}

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_MANAGED_BYTE_STRING_H_
