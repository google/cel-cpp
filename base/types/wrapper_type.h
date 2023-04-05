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

#ifndef THIRD_PARTY_CEL_CPP_BASE_TYPES_WRAPPER_TYPE_H_
#define THIRD_PARTY_CEL_CPP_BASE_TYPES_WRAPPER_TYPE_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/base/attributes.h"
#include "absl/log/absl_check.h"
#include "absl/strings/string_view.h"
#include "base/internal/data.h"
#include "base/kind.h"
#include "base/type.h"
#include "base/types/bool_type.h"
#include "base/types/bytes_type.h"
#include "base/types/double_type.h"
#include "base/types/int_type.h"
#include "base/types/string_type.h"
#include "base/types/uint_type.h"

namespace cel {

class TypeFactory;
class BoolWrapperType;
class BytesWrapperType;
class DoubleWrapperType;
class IntWrapperType;
class StringWrapperType;
class UintWrapperType;

// WrapperType is a special type that is effectively a union of NullType with
// one of BoolType, BytesType, DoubleType, IntType, StringType, or UintType.
class WrapperType : public Type, base_internal::InlineData {
 private:
  using Base = base_internal::InlineData;

 public:
  static constexpr Kind kKind = Kind::kWrapper;

  static bool Is(const Type& type) { return type.kind() == Kind::kWrapper; }

  using Type::Is;

  static const WrapperType& Cast(const Type& type) {
    ABSL_DCHECK(Is(type)) << "cannot cast " << type.name() << " to wrapper";
    return static_cast<const WrapperType&>(type);
  }

  constexpr Kind kind() const { return kKind; }

  absl::string_view name() const;

  std::string DebugString() const { return std::string(name()); }

  const Handle<Type>& wrapped() const;

 private:
  friend class BoolWrapperType;
  friend class BytesWrapperType;
  friend class DoubleWrapperType;
  friend class IntWrapperType;
  friend class StringWrapperType;
  friend class UintWrapperType;

  using Base::Base;
};

inline const Handle<Type>& UnwrapType(const Handle<Type>& handle) {
  return handle->Is<WrapperType>() ? handle.As<WrapperType>()->wrapped()
                                   : handle;
}

inline Handle<Type> UnwrapType(Handle<Type>&& handle) {
  return handle->Is<WrapperType>() ? handle.As<WrapperType>()->wrapped()
                                   : handle;
}

inline const Type& UnwrapType(const Type& type) {
  return WrapperType::Is(type) ? *WrapperType::Cast(type).wrapped() : type;
}

class BoolWrapperType final : public WrapperType {
 public:
  static constexpr absl::string_view kName = "google.protobuf.BoolValue";

  static bool Is(const Type& type) {
    return WrapperType::Is(type) &&
           static_cast<const WrapperType&>(type).wrapped()->kind() ==
               Kind::kBool;
  }

  using WrapperType::Is;

  static const BoolWrapperType& Cast(const Type& type) {
    ABSL_DCHECK(Is(type)) << "cannot cast " << type.name() << " to " << kName;
    return static_cast<const BoolWrapperType&>(type);
  }

  constexpr absl::string_view name() const { return kName; }

  const Handle<BoolType>& wrapped() const { return BoolType::Get(); }

 private:
  friend class TypeFactory;
  template <size_t Size, size_t Align>
  friend class base_internal::AnyData;

  ABSL_ATTRIBUTE_PURE_FUNCTION static const Handle<BoolWrapperType>& Get();

  static constexpr uintptr_t kMetadata =
      base_internal::kStoredInline | base_internal::kTrivial |
      (static_cast<uintptr_t>(kKind) << base_internal::kKindShift) |
      (static_cast<uintptr_t>(BoolType::kKind)
       << base_internal::kInlineVariantShift);

  constexpr BoolWrapperType() : WrapperType(kMetadata) {}
};

class BytesWrapperType final : public WrapperType {
 public:
  static constexpr absl::string_view kName = "google.protobuf.BytesValue";

  static bool Is(const Type& type) {
    return WrapperType::Is(type) &&
           static_cast<const WrapperType&>(type).wrapped()->kind() ==
               Kind::kBytes;
  }

  using WrapperType::Is;

  static const BytesWrapperType& Cast(const Type& type) {
    ABSL_DCHECK(Is(type)) << "cannot cast " << type.name() << " to " << kName;
    return static_cast<const BytesWrapperType&>(type);
  }

  constexpr absl::string_view name() const { return kName; }

  const Handle<BytesType>& wrapped() const { return BytesType::Get(); }

 private:
  friend class TypeFactory;
  template <size_t Size, size_t Align>
  friend class base_internal::AnyData;

  ABSL_ATTRIBUTE_PURE_FUNCTION static const Handle<BytesWrapperType>& Get();

  static constexpr uintptr_t kMetadata =
      base_internal::kStoredInline | base_internal::kTrivial |
      (static_cast<uintptr_t>(kKind) << base_internal::kKindShift) |
      (static_cast<uintptr_t>(BytesType::kKind)
       << base_internal::kInlineVariantShift);

  constexpr BytesWrapperType() : WrapperType(kMetadata) {}
};

class DoubleWrapperType final : public WrapperType {
 public:
  static constexpr absl::string_view kName = "google.protobuf.DoubleValue";

  static bool Is(const Type& type) {
    return WrapperType::Is(type) &&
           static_cast<const WrapperType&>(type).wrapped()->kind() ==
               Kind::kDouble;
  }

  using WrapperType::Is;

  static const DoubleWrapperType& Cast(const Type& type) {
    ABSL_DCHECK(Is(type)) << "cannot cast " << type.name() << " to " << kName;
    return static_cast<const DoubleWrapperType&>(type);
  }

  constexpr absl::string_view name() const { return kName; }

  const Handle<DoubleType>& wrapped() const { return DoubleType::Get(); }

 private:
  friend class TypeFactory;
  template <size_t Size, size_t Align>
  friend class base_internal::AnyData;

  ABSL_ATTRIBUTE_PURE_FUNCTION static const Handle<DoubleWrapperType>& Get();

  static constexpr uintptr_t kMetadata =
      base_internal::kStoredInline | base_internal::kTrivial |
      (static_cast<uintptr_t>(kKind) << base_internal::kKindShift) |
      (static_cast<uintptr_t>(DoubleType::kKind)
       << base_internal::kInlineVariantShift);

  constexpr DoubleWrapperType() : WrapperType(kMetadata) {}
};

class IntWrapperType final : public WrapperType {
 public:
  static constexpr absl::string_view kName = "google.protobuf.Int64Value";

  static bool Is(const Type& type) {
    return WrapperType::Is(type) &&
           static_cast<const WrapperType&>(type).wrapped()->kind() ==
               Kind::kInt;
  }

  using WrapperType::Is;

  static const IntWrapperType& Cast(const Type& type) {
    ABSL_DCHECK(Is(type)) << "cannot cast " << type.name() << " to " << kName;
    return static_cast<const IntWrapperType&>(type);
  }

  constexpr absl::string_view name() const { return kName; }

  const Handle<IntType>& wrapped() const { return IntType::Get(); }

 private:
  friend class TypeFactory;
  template <size_t Size, size_t Align>
  friend class base_internal::AnyData;

  ABSL_ATTRIBUTE_PURE_FUNCTION static const Handle<IntWrapperType>& Get();

  static constexpr uintptr_t kMetadata =
      base_internal::kStoredInline | base_internal::kTrivial |
      (static_cast<uintptr_t>(kKind) << base_internal::kKindShift) |
      (static_cast<uintptr_t>(IntType::kKind)
       << base_internal::kInlineVariantShift);

  constexpr IntWrapperType() : WrapperType(kMetadata) {}
};

class StringWrapperType final : public WrapperType {
 public:
  static constexpr absl::string_view kName = "google.protobuf.StringValue";

  static bool Is(const Type& type) {
    return WrapperType::Is(type) &&
           static_cast<const WrapperType&>(type).wrapped()->kind() ==
               Kind::kString;
  }

  using WrapperType::Is;

  static const StringWrapperType& Cast(const Type& type) {
    ABSL_DCHECK(Is(type)) << "cannot cast " << type.name() << " to " << kName;
    return static_cast<const StringWrapperType&>(type);
  }

  constexpr absl::string_view name() const { return kName; }

  const Handle<StringType>& wrapped() const { return StringType::Get(); }

 private:
  friend class TypeFactory;
  template <size_t Size, size_t Align>
  friend class base_internal::AnyData;

  ABSL_ATTRIBUTE_PURE_FUNCTION static const Handle<StringWrapperType>& Get();

  static constexpr uintptr_t kMetadata =
      base_internal::kStoredInline | base_internal::kTrivial |
      (static_cast<uintptr_t>(kKind) << base_internal::kKindShift) |
      (static_cast<uintptr_t>(StringType::kKind)
       << base_internal::kInlineVariantShift);

  constexpr StringWrapperType() : WrapperType(kMetadata) {}
};

class UintWrapperType final : public WrapperType {
 public:
  static constexpr absl::string_view kName = "google.protobuf.UInt64Value";

  static bool Is(const Type& type) {
    return WrapperType::Is(type) &&
           static_cast<const WrapperType&>(type).wrapped()->kind() ==
               Kind::kUint;
  }

  using WrapperType::Is;

  static const UintWrapperType& Cast(const Type& type) {
    ABSL_DCHECK(Is(type)) << "cannot cast " << type.name() << " to " << kName;
    return static_cast<const UintWrapperType&>(type);
  }

  constexpr absl::string_view name() const { return kName; }

  const Handle<UintType>& wrapped() const { return UintType::Get(); }

 private:
  friend class TypeFactory;
  template <size_t Size, size_t Align>
  friend class base_internal::AnyData;

  ABSL_ATTRIBUTE_PURE_FUNCTION static const Handle<UintWrapperType>& Get();

  static constexpr uintptr_t kMetadata =
      base_internal::kStoredInline | base_internal::kTrivial |
      (static_cast<uintptr_t>(kKind) << base_internal::kKindShift) |
      (static_cast<uintptr_t>(UintType::kKind)
       << base_internal::kInlineVariantShift);

  constexpr UintWrapperType() : WrapperType(kMetadata) {}
};

extern template class Handle<WrapperType>;
extern template class Handle<BoolWrapperType>;
extern template class Handle<BytesWrapperType>;
extern template class Handle<DoubleWrapperType>;
extern template class Handle<IntWrapperType>;
extern template class Handle<StringWrapperType>;
extern template class Handle<UintWrapperType>;

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPES_WRAPPER_TYPE_H_
