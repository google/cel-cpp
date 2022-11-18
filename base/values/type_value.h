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

#include <cstdint>
#include <string>
#include <utility>

#include "absl/hash/hash.h"
#include "base/internal/data.h"
#include "base/kind.h"
#include "base/type.h"
#include "base/types/type_type.h"
#include "base/value.h"

namespace cel {

class TypeValue : public Value {
 public:
  static constexpr Kind kKind = TypeType::kKind;

  static bool Is(const Value& value) { return value.kind() == kKind; }

  constexpr Kind kind() const { return kKind; }

  Handle<TypeType> type() const { return TypeType::Get(); }

  std::string DebugString() const;

  absl::string_view name() const;

  bool Equals(const TypeValue& other) const;

 private:
  friend class ValueHandle;
  template <size_t Size, size_t Align>
  friend class base_internal::AnyData;
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
  friend class base_internal::AnyData;

  static constexpr uintptr_t kMetadata =
      kStoredInline | kTrivial | (static_cast<uintptr_t>(kKind) << kKindShift);

  explicit LegacyTypeValue(absl::string_view value)
      : InlineData(kMetadata), value_(value) {}

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
  friend class base_internal::AnyData;

  static constexpr uintptr_t kMetadata =
      kStoredInline | (static_cast<uintptr_t>(kKind) << kKindShift);

  explicit ModernTypeValue(Handle<Type> value)
      : InlineData(kMetadata), value_(std::move(value)) {}

  ModernTypeValue(const ModernTypeValue&) = default;
  ModernTypeValue(ModernTypeValue&&) = default;
  ModernTypeValue& operator=(const ModernTypeValue&) = default;
  ModernTypeValue& operator=(ModernTypeValue&&) = default;

  Handle<Type> value_;
};

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_TYPE_VALUE_H_
