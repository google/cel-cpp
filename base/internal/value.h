// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_BASE_INTERNAL_VALUE_H_
#define THIRD_PARTY_CEL_CPP_BASE_INTERNAL_VALUE_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/config.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/hash/hash.h"
#include "absl/numeric/bits.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "base/internal/type.h"
#include "base/kind.h"
#include "internal/casts.h"
#include "internal/reference_counted.h"

namespace cel {

class Value;
class Bytes;

namespace base_internal {

// Abstract base class that all non-simple values are derived from. Users will
// not inherit from this directly but rather indirectly through exposed classes
// like cel::Struct.
class BaseValue : public cel::internal::ReferenceCounted {
 public:
  // Returns a human readable representation of this value. The representation
  // is not guaranteed to be consistent across versions and should only be used
  // for debugging purposes.
  virtual std::string DebugString() const = 0;

 protected:
  virtual bool Equals(const cel::Value& value) const = 0;

  virtual void HashValue(absl::HashState state) const = 0;

 private:
  friend class cel::Value;
  friend class cel::Bytes;

  BaseValue() = default;
};

// Type erased state capable of holding a pointer to remote storage or storing
// objects less than two pointers in size inline.
union ExternalDataReleaserState final {
  void* remote;
  alignas(alignof(std::max_align_t)) char local[sizeof(void*) * 2];
};

// Function which deletes the object referenced by ExternalDataReleaserState.
using ExternalDataReleaserDeleter = void(ExternalDataReleaserState* state);

template <typename Releaser>
void LocalExternalDataReleaserDeleter(ExternalDataReleaserState* state) {
  reinterpret_cast<Releaser*>(&state->local)->~Releaser();
}

template <typename Releaser>
void RemoteExternalDataReleaserDeleter(ExternalDataReleaserState* state) {
  ::delete reinterpret_cast<Releaser*>(state->remote);
}

// Function which invokes the object referenced by ExternalDataReleaserState.
using ExternalDataReleaseInvoker =
    void(ExternalDataReleaserState* state) noexcept;

template <typename Releaser>
void LocalExternalDataReleaserInvoker(
    ExternalDataReleaserState* state) noexcept {
  (*reinterpret_cast<Releaser*>(&state->local))();
}

template <typename Releaser>
void RemoteExternalDataReleaserInvoker(
    ExternalDataReleaserState* state) noexcept {
  (*reinterpret_cast<Releaser*>(&state->remote))();
}

struct ExternalDataReleaser final {
  ExternalDataReleaser() = delete;

  template <typename Releaser>
  explicit ExternalDataReleaser(Releaser&& releaser) {
    using DecayedReleaser = std::decay_t<Releaser>;
    if constexpr (sizeof(DecayedReleaser) <= sizeof(void*) * 2 &&
                  alignof(DecayedReleaser) <= alignof(std::max_align_t)) {
      // Object meets size and alignment constraints, will be stored
      // inline in ExternalDataReleaserState.local.
      ::new (static_cast<void*>(&state.local))
          DecayedReleaser(std::forward<Releaser>(releaser));
      invoker = LocalExternalDataReleaserInvoker<DecayedReleaser>;
      if constexpr (std::is_trivially_destructible_v<DecayedReleaser>) {
        // Object is trivially destructable, no need to call destructor at all.
        deleter = nullptr;
      } else {
        deleter = LocalExternalDataReleaserDeleter<DecayedReleaser>;
      }
    } else {
      // Object does not meet size and alignment constraints, allocate on the
      // heap and store pointer in ExternalDataReleaserState::remote. inline in
      // ExternalDataReleaserState::local.
      state.remote = ::new DecayedReleaser(std::forward<Releaser>(releaser));
      invoker = RemoteExternalDataReleaserInvoker<DecayedReleaser>;
      deleter = RemoteExternalDataReleaserDeleter<DecayedReleaser>;
    }
  }

  ExternalDataReleaser(const ExternalDataReleaser&) = delete;

  ExternalDataReleaser(ExternalDataReleaser&&) = delete;

  ~ExternalDataReleaser() {
    (*invoker)(&state);
    if (deleter != nullptr) {
      (*deleter)(&state);
    }
  }

  ExternalDataReleaser& operator=(const ExternalDataReleaser&) = delete;

  ExternalDataReleaser& operator=(ExternalDataReleaser&&) = delete;

  ExternalDataReleaserState state;
  ExternalDataReleaserDeleter* deleter;
  ExternalDataReleaseInvoker* invoker;
};

// Utility class encompassing a contiguous array of data which a function that
// must be called when the data is no longer needed.
struct ExternalData final {
  ExternalData() = delete;

  ExternalData(const void* data, size_t size,
               std::unique_ptr<ExternalDataReleaser> releaser)
      : data(data), size(size), releaser(std::move(releaser)) {}

  ExternalData(const ExternalData&) = delete;

  ExternalData(ExternalData&&) noexcept = default;

  ExternalData& operator=(const ExternalData&) = delete;

  ExternalData& operator=(ExternalData&&) noexcept = default;

  const void* data;
  size_t size;
  std::unique_ptr<ExternalDataReleaser> releaser;
};

// Currently absl::Status has a size that is less than or equal to 8, however
// this could change at any time. Thus we delegate the lifetime management to
// BaseInlinedStatus which is always less than or equal to 8 bytes.
template <bool Small>
class BaseInlinedStatus;

// Specialization for when the size of absl::Status is less than or equal to 8
// bytes.
template <>
class BaseInlinedStatus<true> final {
 public:
  BaseInlinedStatus() = default;

  BaseInlinedStatus(const BaseInlinedStatus&) = default;

  BaseInlinedStatus(BaseInlinedStatus&&) = default;

  explicit BaseInlinedStatus(const absl::Status& status) : status_(status) {}

  BaseInlinedStatus& operator=(const BaseInlinedStatus&) = default;

  BaseInlinedStatus& operator=(BaseInlinedStatus&&) = default;

  BaseInlinedStatus& operator=(const absl::Status& status) {
    status_ = status;
    return *this;
  }

  const absl::Status& status() const { return status_; }

 private:
  absl::Status status_;
};

// Specialization for when the size of absl::Status is greater than 8 bytes. As
// mentioned above, this template is never used today. It could in the future if
// the size of `absl::Status` ever changes. Without this specialization, our
// static asserts below would break and so would compiling CEL.
template <>
class BaseInlinedStatus<false> final {
 public:
  BaseInlinedStatus() = default;

  BaseInlinedStatus(const BaseInlinedStatus&) = default;

  BaseInlinedStatus(BaseInlinedStatus&&) = default;

  explicit BaseInlinedStatus(const absl::Status& status)
      : status_(std::make_shared<absl::Status>(status)) {}

  BaseInlinedStatus& operator=(const BaseInlinedStatus&) = default;

  BaseInlinedStatus& operator=(BaseInlinedStatus&&) = default;

  BaseInlinedStatus& operator=(const absl::Status& status) {
    if (status_) {
      *status_ = status;
    } else {
      status_ = std::make_shared<absl::Status>(status);
    }
    return *this;
  }

  const absl::Status& status() const {
    static const absl::Status* ok_status = new absl::Status();
    return status_ ? *status_ : *ok_status;
  }

 private:
  std::shared_ptr<absl::Status> status_;
};

using InlinedStatus = BaseInlinedStatus<(sizeof(absl::Status) <= 8)>;

// ValueMetadata is a specialized tagged union capable of storing either a
// pointer to a BaseType or a Kind. Only simple kinds are stored directly.
// Simple kinds can be converted into cel::Type using cel::Type::Simple.
// ValueMetadata is primarily used to interpret the contents of ValueContent.
//
// We assume that all pointers returned by `malloc()` are at minimum aligned to
// 4 bytes. In practice this assumption is pretty safe and all known
// implementations exhibit this behavior.
//
// The tagged union byte layout depends on the 0 bit.
//
// Bit 0 unset:
//
// --------------------------------
// | 63 ... 2 |    1     |    0   |
// --------------------------------
// | pointer  | reserved | reffed |
// --------------------------------
//
// Bit 0 set:
//
// ---------------------------------------------------------------
// |     63 ... 32    | 31 ... 16 | 15 ... 8 |  7 ... 1 |   0    |
// ---------------------------------------------------------------
// | extended_content |  reserved |    kind  | reserved | simple |
// ---------------------------------------------------------------
//
// Q: Why not use absl::variant/std::variant?
// A: In theory, we could. However it would be repetative and inefficient.
// variant has a size equal to the largest of its memory types plus an
// additional field keeping track of the type that is active. For our purposes,
// the field that is active is kept track of by ValueMetadata and the storage in
// ValueContent. We know what is stored in ValueContent by the kind/type in
// ValueMetadata. Since we need to keep the type bundled with the Value, using
// variant would introduce two sources of truth for what is stored in
// ValueContent. If we chose the naive implementation, which would be to use
// Type instead of ValueMetadata and variant instead of ValueContent, each time
// we copy Value we would be guaranteed to incur a reference count causing a
// cache miss. This approach avoids that reference count for simple types.
// Additionally the size of Value would now be roughly 8 + 16 on 64-bit
// platforms.
//
// As with ValueContent, this class is only meant to be used by cel::Value.
class ValueMetadata final {
 public:
  constexpr ValueMetadata() : raw_(MakeDefault()) {}

  constexpr explicit ValueMetadata(Kind kind) : ValueMetadata(kind, 0) {}

  constexpr ValueMetadata(Kind kind, uint32_t extended_content)
      : raw_(MakeSimple(kind, extended_content)) {}

  explicit ValueMetadata(const BaseType* base_type)
      : ptr_(reinterpret_cast<uintptr_t>(base_type)) {
    // Assert that the lower 2 bits are 0, a.k.a. at minimum 4 byte aligned.
    ABSL_ASSERT(absl::countr_zero(reinterpret_cast<uintptr_t>(base_type)) >= 2);
  }

  ValueMetadata(const ValueMetadata&) = delete;

  ValueMetadata(ValueMetadata&&) = delete;

  ValueMetadata& operator=(const ValueMetadata&) = delete;

  ValueMetadata& operator=(ValueMetadata&&) = delete;

  constexpr bool simple_tag() const {
    return (lower_ & kSimpleTag) == kSimpleTag;
  }

  constexpr uint32_t extended_content() const {
    ABSL_ASSERT(simple_tag());
    return higher_;
  }

  const BaseType* base_type() const {
    ABSL_ASSERT(!simple_tag());
    return reinterpret_cast<const BaseType*>(ptr_ & kPtrMask);
  }

  Kind kind() const {
    return simple_tag() ? static_cast<Kind>(lower_ >> 8) : base_type()->kind();
  }

  void Reset() {
    if (!simple_tag()) {
      internal::Unref(base_type());
    }
    raw_ = MakeDefault();
  }

  void CopyFrom(const ValueMetadata& other) {
    if (ABSL_PREDICT_TRUE(this != std::addressof(other))) {
      if (!other.simple_tag()) {
        internal::Ref(other.base_type());
      }
      if (!simple_tag()) {
        internal::Unref(base_type());
      }
      raw_ = other.raw_;
    }
  }

  void MoveFrom(ValueMetadata&& other) {
    if (ABSL_PREDICT_TRUE(this != std::addressof(other))) {
      if (!simple_tag()) {
        internal::Unref(base_type());
      }
      raw_ = other.raw_;
      other.raw_ = MakeDefault();
    }
  }

 private:
  static constexpr uint64_t MakeSimple(Kind kind, uint32_t extended_content) {
    return static_cast<uint64_t>(kSimpleTag |
                                 (static_cast<uint32_t>(kind) << 8)) |
           (static_cast<uint64_t>(extended_content) << 32);
  }

  static constexpr uint64_t MakeDefault() {
    return MakeSimple(Kind::kNullType, 0);
  }

  static constexpr uint32_t kNoTag = 0;
  static constexpr uint32_t kSimpleTag =
      1 << 0;  // Indicates the kind is simple and there is no BaseType* held.
  static constexpr uint32_t kReservedTag = 1 << 1;
  static constexpr uintptr_t kPtrMask =
      ~static_cast<uintptr_t>(kSimpleTag | kReservedTag);

  union {
    uint64_t raw_;

#if defined(ABSL_IS_LITTLE_ENDIAN)
    struct {
      uint32_t lower_;
      uint32_t higher_;
    };
#elif defined(ABSL_IS_BIG_ENDIAN)
    struct {
      uint32_t higher_;
      uint32_t lower_;
    };
#else
#error "Platform is neither big endian nor little endian"
#endif

    uintptr_t ptr_;
  };
};

static_assert(sizeof(ValueMetadata) == 8,
              "Expected sizeof(ValueMetadata) to be 8");

// ValueContent is an untagged union whose contents are determined by the
// accompanying ValueMetadata.
//
// As with ValueMetadata, this class is only meant to be used by cel::Value.
class ValueContent final {
 public:
  constexpr ValueContent() : raw_(0) {}

  constexpr explicit ValueContent(bool value) : bool_value_(value) {}

  constexpr explicit ValueContent(int64_t value) : int_value_(value) {}

  constexpr explicit ValueContent(uint64_t value) : uint_value_(value) {}

  constexpr explicit ValueContent(double value) : double_value_(value) {}

  explicit ValueContent(const absl::Status& status) {
    construct_error_value(status);
  }

  constexpr explicit ValueContent(BaseValue* base_value)
      : base_value_(base_value) {}

  ValueContent(const ValueContent&) = delete;

  ValueContent(ValueContent&&) = delete;

  ~ValueContent() {}

  ValueContent& operator=(const ValueContent&) = delete;

  ValueContent& operator=(ValueContent&&) = delete;

  constexpr bool bool_value() const { return bool_value_; }

  constexpr int64_t int_value() const { return int_value_; }

  constexpr uint64_t uint_value() const { return uint_value_; }

  constexpr double double_value() const { return double_value_; }

  constexpr void construct_trivial_value(uint64_t value) { raw_ = value; }

  constexpr void destruct_trivial_value() { raw_ = 0; }

  constexpr uint64_t trivial_value() const { return raw_; }

  // Updates this to hold `value`, incrementing the reference count. This is
  // used during copies.
  void construct_reffed_value(BaseValue* value) {
    base_value_ = cel::internal::Ref(value);
  }

  // Updates this to hold `value` without incrementing the reference count. This
  // is used during moves.
  void adopt_reffed_value(BaseValue* value) { base_value_ = value; }

  // Decrement the reference count of the currently held reffed value and clear
  // this.
  void destruct_reffed_value() {
    cel::internal::Unref(base_value_);
    base_value_ = nullptr;
  }

  // Return the currently held reffed value and reset this, without decrementing
  // the reference count. This is used during moves.
  BaseValue* release_reffed_value() {
    BaseValue* reffed_value = base_value_;
    base_value_ = nullptr;
    return reffed_value;
  }

  constexpr BaseValue* reffed_value() const { return base_value_; }

  void construct_error_value(const absl::Status& status) {
    ::new (static_cast<void*>(std::addressof(error_value_)))
        InlinedStatus(status);
  }

  void assign_error_value(const absl::Status& status) { error_value_ = status; }

  void destruct_error_value() {
    std::addressof(error_value_)->~InlinedStatus();
  }

  constexpr const absl::Status& error_value() const {
    return error_value_.status();
  }

 private:
  union {
    uint64_t raw_;

    bool bool_value_;
    int64_t int_value_;
    uint64_t uint_value_;
    double double_value_;
    InlinedStatus error_value_;
    BaseValue* base_value_;
  };
};

static_assert(sizeof(ValueContent) == 8,
              "Expected sizeof(ValueContent) to be 8");

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_INTERNAL_VALUE_H_
