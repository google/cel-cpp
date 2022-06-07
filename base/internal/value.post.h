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

// IWYU pragma: private, include "base/value.h"

#ifndef THIRD_PARTY_CEL_CPP_BASE_INTERNAL_VALUE_POST_H_
#define THIRD_PARTY_CEL_CPP_BASE_INTERNAL_VALUE_POST_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/hash/hash.h"
#include "absl/numeric/bits.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "base/handle.h"
#include "internal/casts.h"

namespace cel {

namespace base_internal {

inline internal::TypeInfo GetEnumValueTypeId(const EnumValue& enum_value) {
  return enum_value.TypeId();
}

inline internal::TypeInfo GetStructValueTypeId(
    const StructValue& struct_value) {
  return struct_value.TypeId();
}

inline internal::TypeInfo GetListValueTypeId(const ListValue& list_value) {
  return list_value.TypeId();
}

inline internal::TypeInfo GetMapValueTypeId(const MapValue& map_value) {
  return map_value.TypeId();
}

// Implementation of BytesValue that is stored inlined within a handle. Since
// absl::Cord is reference counted itself, this is more efficient than storing
// this on the heap.
class InlinedCordBytesValue final : public BytesValue, public ResourceInlined {
 private:
  template <HandleType H>
  friend class ValueHandle;

  explicit InlinedCordBytesValue(absl::Cord value) : value_(std::move(value)) {}

  InlinedCordBytesValue() = delete;

  InlinedCordBytesValue(const InlinedCordBytesValue&) = default;
  InlinedCordBytesValue(InlinedCordBytesValue&&) = default;

  // See comments for respective member functions on `ByteValue` and `Value`.
  void CopyTo(Value& address) const override;
  void MoveTo(Value& address) override;
  absl::Cord ToCord(bool reference_counted) const override;
  Rep rep() const override;

  absl::Cord value_;
};

// Implementation of BytesValue that is stored inlined within a handle. This
// class is inheritently unsafe and care should be taken when using it.
// Typically this should only be used for empty strings or data that is static
// and lives for the duration of a program.
class InlinedStringViewBytesValue final : public BytesValue,
                                          public ResourceInlined {
 private:
  template <HandleType H>
  friend class ValueHandle;

  explicit InlinedStringViewBytesValue(absl::string_view value)
      : value_(value) {}

  InlinedStringViewBytesValue() = delete;

  InlinedStringViewBytesValue(const InlinedStringViewBytesValue&) = default;
  InlinedStringViewBytesValue(InlinedStringViewBytesValue&&) = default;

  // See comments for respective member functions on `ByteValue` and `Value`.
  void CopyTo(Value& address) const override;
  void MoveTo(Value& address) override;
  absl::Cord ToCord(bool reference_counted) const override;
  Rep rep() const override;

  absl::string_view value_;
};

// Implementation of BytesValue that uses std::string and is allocated on the
// heap, potentially reference counted.
class StringBytesValue final : public BytesValue {
 private:
  friend class cel::MemoryManager;

  explicit StringBytesValue(std::string value) : value_(std::move(value)) {}

  StringBytesValue() = delete;
  StringBytesValue(const StringBytesValue&) = delete;
  StringBytesValue(StringBytesValue&&) = delete;

  // See comments for respective member functions on `ByteValue` and `Value`.
  std::pair<size_t, size_t> SizeAndAlignment() const override;
  absl::Cord ToCord(bool reference_counted) const override;
  Rep rep() const override;

  std::string value_;
};

// Implementation of BytesValue that wraps a contiguous array of bytes and calls
// the releaser when it is no longer needed. It is stored on the heap and
// potentially reference counted.
class ExternalDataBytesValue final : public BytesValue {
 private:
  friend class cel::MemoryManager;

  explicit ExternalDataBytesValue(ExternalData value)
      : value_(std::move(value)) {}

  ExternalDataBytesValue() = delete;
  ExternalDataBytesValue(const ExternalDataBytesValue&) = delete;
  ExternalDataBytesValue(ExternalDataBytesValue&&) = delete;

  // See comments for respective member functions on `ByteValue` and `Value`.
  std::pair<size_t, size_t> SizeAndAlignment() const override;
  absl::Cord ToCord(bool reference_counted) const override;
  Rep rep() const override;

  ExternalData value_;
};

// Implementation of StringValue that is stored inlined within a handle. Since
// absl::Cord is reference counted itself, this is more efficient then storing
// this on the heap.
class InlinedCordStringValue final : public StringValue,
                                     public ResourceInlined {
 private:
  template <HandleType H>
  friend class ValueHandle;

  explicit InlinedCordStringValue(absl::Cord value)
      : InlinedCordStringValue(0, std::move(value)) {}

  InlinedCordStringValue(size_t size, absl::Cord value)
      : StringValue(size), value_(std::move(value)) {}

  InlinedCordStringValue() = delete;

  InlinedCordStringValue(const InlinedCordStringValue&) = default;
  InlinedCordStringValue(InlinedCordStringValue&&) = default;

  // See comments for respective member functions on `StringValue` and `Value`.
  void CopyTo(Value& address) const override;
  void MoveTo(Value& address) override;
  absl::Cord ToCord(bool reference_counted) const override;
  Rep rep() const override;

  absl::Cord value_;
};

// Implementation of StringValue that is stored inlined within a handle. This
// class is inheritently unsafe and care should be taken when using it.
// Typically this should only be used for empty strings or data that is static
// and lives for the duration of a program.
class InlinedStringViewStringValue final : public StringValue,
                                           public ResourceInlined {
 private:
  template <HandleType H>
  friend class ValueHandle;

  explicit InlinedStringViewStringValue(absl::string_view value)
      : InlinedStringViewStringValue(0, value) {}

  InlinedStringViewStringValue(size_t size, absl::string_view value)
      : StringValue(size), value_(value) {}

  InlinedStringViewStringValue() = delete;

  InlinedStringViewStringValue(const InlinedStringViewStringValue&) = default;
  InlinedStringViewStringValue(InlinedStringViewStringValue&&) = default;

  // See comments for respective member functions on `StringValue` and `Value`.
  void CopyTo(Value& address) const override;
  void MoveTo(Value& address) override;
  absl::Cord ToCord(bool reference_counted) const override;
  Rep rep() const override;

  absl::string_view value_;
};

// Implementation of StringValue that uses std::string and is allocated on the
// heap, potentially reference counted.
class StringStringValue final : public StringValue {
 private:
  friend class cel::MemoryManager;

  explicit StringStringValue(std::string value)
      : StringStringValue(0, std::move(value)) {}

  StringStringValue(size_t size, std::string value)
      : StringValue(size), value_(std::move(value)) {}

  StringStringValue() = delete;
  StringStringValue(const StringStringValue&) = delete;
  StringStringValue(StringStringValue&&) = delete;

  // See comments for respective member functions on `StringValue` and `Value`.
  std::pair<size_t, size_t> SizeAndAlignment() const override;
  absl::Cord ToCord(bool reference_counted) const override;
  Rep rep() const override;

  std::string value_;
};

// Implementation of StringValue that wraps a contiguous array of bytes and
// calls the releaser when it is no longer needed. It is stored on the heap and
// potentially reference counted.
class ExternalDataStringValue final : public StringValue {
 private:
  friend class cel::MemoryManager;

  explicit ExternalDataStringValue(ExternalData value)
      : ExternalDataStringValue(0, std::move(value)) {}

  ExternalDataStringValue(size_t size, ExternalData value)
      : StringValue(size), value_(std::move(value)) {}

  ExternalDataStringValue() = delete;
  ExternalDataStringValue(const ExternalDataStringValue&) = delete;
  ExternalDataStringValue(ExternalDataStringValue&&) = delete;

  // See comments for respective member functions on `StringValue` and `Value`.
  std::pair<size_t, size_t> SizeAndAlignment() const override;
  absl::Cord ToCord(bool reference_counted) const override;
  Rep rep() const override;

  ExternalData value_;
};

struct ABSL_ATTRIBUTE_UNUSED CheckVptrOffsetBase {
  virtual ~CheckVptrOffsetBase() = default;

  virtual void Member() const {}
};

// Class used to assert the object memory layout for vptr at compile time,
// otherwise it is unused.
struct ABSL_ATTRIBUTE_UNUSED CheckVptrOffset final
    : public CheckVptrOffsetBase {
  uintptr_t member;
};

// Ensure the hidden vptr is stored at the beginning of the object. See
// ValueHandleData for more information.
static_assert(offsetof(CheckVptrOffset, member) == sizeof(void*),
              "CEL C++ requires a compiler that stores the vptr as a hidden "
              "member at the beginning of the object. If this static_assert "
              "fails, please reach out to the CEL team.");

// Union of all known inlinable values.
union ValueHandleData final {
  // As asserted above, we rely on the fact that the compiler stores the vptr as
  // a hidden member at the beginning of the object. We then re-use the first 2
  // bits to differentiate between an inlined value (both 0), a heap allocated
  // reference counted value, or a arena allocated value.
  void* vptr;
  std::aligned_union_t<sizeof(void*), NullValue, ErrorValue, BoolValue,
                       IntValue, UintValue, DoubleValue, InlinedCordBytesValue,
                       InlinedStringViewBytesValue, InlinedCordStringValue,
                       InlinedStringViewStringValue, DurationValue,
                       TimestampValue>
      padding;
};

// Base implementation of persistent and transient handles for values. This
// contains implementation details shared among both, but is never used
// directly. The derived classes are responsible for defining appropriate
// constructors and assignments.
class ValueHandleBase {
 public:
  ValueHandleBase() { Reset(); }

  // Used by derived classes to bypass default construction to perform their own
  // construction.
  explicit ValueHandleBase(HandleInPlace) {}

  // Called by `Transient` and `Persistent` to implement the same operator. They
  // will handle enforcing const correctness.
  Value& operator*() const { return get(); }

  // Called by `Transient` and `Persistent` to implement the same operator. They
  // will handle enforcing const correctness.
  Value* operator->() const { return std::addressof(get()); }

  // Called by internal accessors `base_internal::IsXHandle`.
  bool IsManaged() const { return (vptr() & kValueHandleManaged) != 0; }

  // Called by internal accessors `base_internal::IsXHandle`.
  bool IsUnmanaged() const { return (vptr() & kValueHandleUnmanaged) != 0; }

  // Called by internal accessors `base_internal::IsXHandle`.
  bool IsInlined() const { return (vptr() & kValueHandleBits) == 0; }

  // Called by `Transient` and `Persistent` to implement the same function.
  template <typename T>
  bool Is() const {
    // Tests that this is not an empty handle and then dereferences the handle
    // calling the RTTI-like implementation T::Is which takes `const Value&`.
    return static_cast<bool>(*this) && T::Is(static_cast<const Value&>(**this));
  }

  // Called by `Transient` and `Persistent` to implement the same operator.
  explicit operator bool() const { return (vptr() & kValueHandleMask) != 0; }

  // Called by `Transient` and `Persistent` to implement the same operator.
  friend bool operator==(const ValueHandleBase& lhs,
                         const ValueHandleBase& rhs) {
    const Value& lhs_value = ABSL_PREDICT_TRUE(static_cast<bool>(lhs))
                                 ? lhs.get()
                                 : static_cast<const Value&>(NullValue::Get());
    const Value& rhs_value = ABSL_PREDICT_TRUE(static_cast<bool>(rhs))
                                 ? rhs.get()
                                 : static_cast<const Value&>(NullValue::Get());
    return lhs_value.Equals(rhs_value);
  }

  // Called by `Transient` and `Persistent` to implement std::swap.
  friend void swap(ValueHandleBase& lhs, ValueHandleBase& rhs) {
    if (lhs.empty_or_not_inlined() && rhs.empty_or_not_inlined()) {
      // Both `lhs` and `rhs` are simple pointers. Just swap them.
      std::swap(lhs.data_.vptr, rhs.data_.vptr);
      return;
    }
    ValueHandleBase tmp;
    Move(lhs, tmp);
    Move(rhs, lhs);
    Move(tmp, rhs);
  }

  template <typename H>
  friend H AbslHashValue(H state, const ValueHandleBase& handle) {
    if (ABSL_PREDICT_TRUE(static_cast<bool>(handle))) {
      handle.get().HashValue(absl::HashState::Create(&state));
    } else {
      NullValue::Get().HashValue(absl::HashState::Create(&state));
    }
    return state;
  }

 private:
  template <HandleType H>
  friend class ValueHandle;

  // Resets the state to the same as the default constructor. Does not perform
  // any destruction of existing content.
  void Reset() { data_.vptr = reinterpret_cast<void*>(kValueHandleUnmanaged); }

  void Unref() const {
    ABSL_ASSERT(reffed());
    reinterpret_cast<const Value*>(vptr() & kValueHandleMask)->Unref();
  }

  void Ref() const {
    ABSL_ASSERT(reffed());
    reinterpret_cast<const Value*>(vptr() & kValueHandleMask)->Ref();
  }

  Value& get() const {
    return *(inlined()
                 ? reinterpret_cast<Value*>(const_cast<void**>(&data_.vptr))
                 : reinterpret_cast<Value*>(vptr() & kValueHandleMask));
  }

  bool empty() const { return !static_cast<bool>(*this); }

  // Does the stored data represent an inlined value?
  bool inlined() const { return (vptr() & kValueHandleBits) == 0; }

  // Does the stored data represent a non-null inlined value?
  bool not_empty_and_inlined() const {
    return (vptr() & kValueHandleBits) == 0 && (vptr() & kValueHandleMask) != 0;
  }

  // Does the stored data represent null, heap allocated reference counted, or
  // arena allocated value?
  bool empty_or_not_inlined() const {
    return (vptr() & kValueHandleBits) != 0 || (vptr() & kValueHandleMask) == 0;
  }

  // Does the stored data required reference counting?
  bool reffed() const { return (vptr() & kValueHandleManaged) != 0; }

  uintptr_t vptr() const { return reinterpret_cast<uintptr_t>(data_.vptr); }

  static void Copy(const ValueHandleBase& from, ValueHandleBase& to) {
    if (from.empty_or_not_inlined()) {
      // `from` is a simple pointer, just copy it.
      to.data_.vptr = from.data_.vptr;
    } else {
      from.get().CopyTo(*reinterpret_cast<Value*>(&to.data_.vptr));
    }
  }

  static void Move(ValueHandleBase& from, ValueHandleBase& to) {
    if (from.empty_or_not_inlined()) {
      // `from` is a simple pointer, just swap it.
      std::swap(from.data_.vptr, to.data_.vptr);
    } else {
      from.get().MoveTo(*reinterpret_cast<Value*>(&to.data_.vptr));
      DestructInlined(from);
    }
  }

  static void DestructInlined(ValueHandleBase& handle) {
    ABSL_ASSERT(!handle.empty_or_not_inlined());
    handle.get().~Value();
    handle.Reset();
  }

  ValueHandleData data_;
};

// All methods are called by `Persistent`.
template <>
class ValueHandle<HandleType::kPersistent> final : public ValueHandleBase {
 private:
  using Base = ValueHandleBase;

 public:
  ValueHandle() = default;

  template <typename T, typename... Args>
  explicit ValueHandle(InlinedResource<T>, Args&&... args)
      : ValueHandleBase(kHandleInPlace) {
    static_assert(sizeof(T) <= sizeof(data_.padding),
                  "T cannot be inlined in Handle");
    static_assert(alignof(T) <= alignof(data_.padding),
                  "T cannot be inlined in Handle");
    ::new (const_cast<void*>(static_cast<const volatile void*>(&data_.padding)))
        T(std::forward<Args>(args)...);
    ABSL_ASSERT(absl::countr_zero(vptr()) >=
                2);  // Verify the lower 2 bits are available.
  }

  template <typename T, typename F>
  ValueHandle(UnmanagedResource<T>, F& from) : ValueHandleBase(kHandleInPlace) {
    uintptr_t vptr = reinterpret_cast<uintptr_t>(
        static_cast<const Value*>(static_cast<const T*>(std::addressof(from))));
    ABSL_ASSERT(absl::countr_zero(vptr) >=
                2);  // Verify the lower 2 bits are available.
    data_.vptr = reinterpret_cast<void*>(vptr | kValueHandleUnmanaged);
  }

  template <typename T, typename F>
  ValueHandle(ManagedResource<T>, F& from) : ValueHandleBase(kHandleInPlace) {
    uintptr_t vptr = reinterpret_cast<uintptr_t>(
        static_cast<const Value*>(static_cast<const T*>(std::addressof(from))));
    ABSL_ASSERT(absl::countr_zero(vptr) >=
                2);  // Verify the lower 2 bits are available.
    data_.vptr = reinterpret_cast<void*>(vptr | kValueHandleManaged);
  }

  ValueHandle(const PersistentValueHandle& other) : ValueHandle() {
    Base::Copy(other, *this);
    if (reffed()) {
      Ref();
    }
  }

  ValueHandle(PersistentValueHandle&& other) : ValueHandle() {
    Base::Move(other, *this);
  }

  ~ValueHandle() {
    if (not_empty_and_inlined()) {
      DestructInlined(*this);
    } else if (reffed()) {
      Unref();
    }
  }

  ValueHandle& operator=(const PersistentValueHandle& other) {
    if (not_empty_and_inlined()) {
      DestructInlined(*this);
    } else if (reffed()) {
      Unref();
    }
    Base::Copy(other, *this);
    if (reffed()) {
      Ref();
    }
    return *this;
  }

  ValueHandle& operator=(PersistentValueHandle&& other) {
    if (not_empty_and_inlined()) {
      DestructInlined(*this);
    } else if (reffed()) {
      Unref();
      Reset();
    }
    Base::Move(other, *this);
    return *this;
  }
};
// Specialization for Value providing the implementation to `Persistent`.
template <>
struct HandleTraits<HandleType::kPersistent, Value> {
  using handle_type = ValueHandle<HandleType::kPersistent>;
};

// Partial specialization for `Persistent` for all classes derived from Value.
template <typename T>
struct HandleTraits<HandleType::kPersistent, T,
                    std::enable_if_t<(std::is_base_of_v<Value, T> &&
                                      !std::is_same_v<Value, T>)>>
    final : public HandleTraits<HandleType::kPersistent, Value> {};

}  // namespace base_internal

#define CEL_INTERNAL_VALUE_DECL(name)          \
  extern template class Persistent<name>;      \
  extern template class Persistent<const name>
CEL_INTERNAL_VALUE_DECL(Value);
CEL_INTERNAL_VALUE_DECL(NullValue);
CEL_INTERNAL_VALUE_DECL(ErrorValue);
CEL_INTERNAL_VALUE_DECL(BoolValue);
CEL_INTERNAL_VALUE_DECL(IntValue);
CEL_INTERNAL_VALUE_DECL(UintValue);
CEL_INTERNAL_VALUE_DECL(DoubleValue);
CEL_INTERNAL_VALUE_DECL(BytesValue);
CEL_INTERNAL_VALUE_DECL(StringValue);
CEL_INTERNAL_VALUE_DECL(DurationValue);
CEL_INTERNAL_VALUE_DECL(TimestampValue);
CEL_INTERNAL_VALUE_DECL(EnumValue);
CEL_INTERNAL_VALUE_DECL(StructValue);
CEL_INTERNAL_VALUE_DECL(ListValue);
CEL_INTERNAL_VALUE_DECL(MapValue);
CEL_INTERNAL_VALUE_DECL(TypeValue);
#undef CEL_INTERNAL_VALUE_DECL

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_INTERNAL_VALUE_POST_H_