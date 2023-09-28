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

#ifndef THIRD_PARTY_CEL_CPP_BASE_VALUES_TYPE_VALUE_H_
#define THIRD_PARTY_CEL_CPP_BASE_VALUES_TYPE_VALUE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/log/absl_check.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/handle.h"
#include "base/internal/data.h"
#include "base/kind.h"
#include "base/type.h"
#include "base/types/type_type.h"
#include "base/value.h"
#include "common/any.h"
#include "common/json.h"

namespace cel {

class TypeValue : public Value {
 public:
  static constexpr ValueKind kKind = ValueKind::kType;

  static bool Is(const Value& value) { return value.kind() == kKind; }

  using Value::Is;

  static const TypeValue& Cast(const Value& value) {
    ABSL_DCHECK(Is(value)) << "cannot cast " << value.type()->name()
                           << " to type";
    return static_cast<const TypeValue&>(value);
  }

  constexpr ValueKind kind() const { return kKind; }

  Handle<TypeType> type() const { return TypeType::Get(); }

  std::string DebugString() const;

  absl::StatusOr<Any> ConvertToAny(ValueFactory&) const;

  absl::StatusOr<Json> ConvertToJson(ValueFactory&) const;

  absl::StatusOr<Handle<Value>> ConvertToType(ValueFactory& value_factory,
                                              const Handle<Type>& type) const;

  absl::StatusOr<Handle<Value>> Equals(ValueFactory& value_factory,
                                       const Value& other) const;

  absl::string_view name() const;

 private:
  friend class ValueHandle;
  template <size_t Size, size_t Align>
  friend struct base_internal::AnyData;
  friend class base_internal::LegacyTypeValue;
  friend class base_internal::ModernTypeValue;

  TypeValue() = default;
  TypeValue(const TypeValue&) = default;
  TypeValue(TypeValue&&) = default;
  TypeValue& operator=(const TypeValue&) = default;
  TypeValue& operator=(TypeValue&&) = default;
};

CEL_INTERNAL_VALUE_DECL(TypeValue);

namespace base_internal {

class LegacyTypeValue final : public TypeValue, InlineData {
 public:
  absl::string_view name() const { return value_; }

 private:
  friend class ValueHandle;
  template <size_t Size, size_t Align>
  friend struct base_internal::AnyData;

  static constexpr uintptr_t kMetadata =
      kStoredInline | kTrivial | (static_cast<uintptr_t>(kKind) << kKindShift);

  explicit LegacyTypeValue(absl::string_view value)
      : InlineData(kMetadata |
                   AsInlineVariant(InlinedTypeValueVariant::kLegacy)),
        value_(value) {}

  LegacyTypeValue(const LegacyTypeValue&) = default;
  LegacyTypeValue(LegacyTypeValue&&) = default;
  LegacyTypeValue& operator=(const LegacyTypeValue&) = default;
  LegacyTypeValue& operator=(LegacyTypeValue&&) = default;

  absl::string_view value_;
};

class ModernTypeValue final : public TypeValue, InlineData {
 public:
  absl::string_view name() const { return value_->name(); }

 private:
  friend class ValueHandle;
  template <size_t Size, size_t Align>
  friend struct base_internal::AnyData;

  static constexpr uintptr_t kMetadata =
      kStoredInline | (static_cast<uintptr_t>(kKind) << kKindShift);

  static uintptr_t AdditionalMetadata(const Type& type) {
    static_assert(
        std::is_base_of_v<base_internal::InlineData, LegacyTypeValue>,
        "This logic relies on the fact that EnumValue is stored inline");
    // Because LegacyTypeValue is stored inline and has only one member, we can
    // be considered trivial if Handle<Type> has a skippable destructor.
    return Metadata::IsDestructorSkippable(type) ? kTrivial : uintptr_t{0};
  }

  explicit ModernTypeValue(Handle<Type> value)
      : InlineData(kMetadata | AdditionalMetadata(*value) |
                   AsInlineVariant(InlinedTypeValueVariant::kModern)),
        value_(std::move(value)) {}

  ModernTypeValue(const ModernTypeValue&) = default;
  ModernTypeValue(ModernTypeValue&&) = default;
  ModernTypeValue& operator=(const ModernTypeValue&) = default;
  ModernTypeValue& operator=(ModernTypeValue&&) = default;

  Handle<Type> value_;
};

}  // namespace base_internal

namespace base_internal {

template <>
struct ValueTraits<TypeValue> {
  using type = TypeValue;

  using type_type = TypeType;

  using underlying_type = void;

  static std::string DebugString(const type& value) {
    return value.DebugString();
  }

  static Handle<type> Wrap(ValueFactory& value_factory, Handle<type> value) {
    static_cast<void>(value_factory);
    return value;
  }

  static Handle<type> Unwrap(Handle<type> value) { return value; }
};

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_TYPE_VALUE_H_
