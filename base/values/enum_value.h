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

#ifndef THIRD_PARTY_CEL_CPP_BASE_VALUES_ENUM_VALUE_H_
#define THIRD_PARTY_CEL_CPP_BASE_VALUES_ENUM_VALUE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "base/internal/data.h"
#include "base/kind.h"
#include "base/type.h"
#include "base/types/enum_type.h"
#include "base/value.h"

namespace cel {

class ValueFactory;

// EnumValue represents a single constant belonging to cel::EnumType.
class EnumValue final : public Value, public base_internal::InlineData {
 public:
  static constexpr Kind kKind = EnumType::kKind;

  static bool Is(const Value& value) { return value.kind() == kKind; }

  using ConstantId = EnumType::ConstantId;

  constexpr Kind kind() const { return kKind; }

  Handle<EnumType> type() const { return type_; }

  std::string DebugString() const;

  constexpr int64_t number() const { return number_; }

  absl::string_view name() const;

 private:
  friend class base_internal::ValueHandle;
  template <size_t Size, size_t Align>
  friend class base_internal::AnyData;

  static constexpr uintptr_t kMetadata =
      base_internal::kStoredInline |
      (static_cast<uintptr_t>(kKind) << base_internal::kKindShift);

  EnumValue(Handle<EnumType> type, int64_t number)
      : base_internal::InlineData(kMetadata),
        type_(std::move(type)),
        number_(number) {}

  Handle<EnumType> type_;
  int64_t number_;
};

CEL_INTERNAL_VALUE_DECL(EnumValue);

namespace base_internal {

template <>
struct ValueTraits<EnumValue> {
  using type = EnumValue;

  using type_type = EnumType;

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

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_ENUM_VALUE_H_
