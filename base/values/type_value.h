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
#include "base/kind.h"
#include "base/type.h"
#include "base/types/type_type.h"
#include "base/value.h"

namespace cel {

class TypeValue final : public Value, public base_internal::InlineData {
 public:
  static constexpr Kind kKind = TypeType::kKind;

  static bool Is(const Value& value) { return value.kind() == kKind; }

  constexpr Kind kind() const { return kKind; }

  Persistent<const TypeType> type() const { return TypeType::Get(); }

  std::string DebugString() const;

  void HashValue(absl::HashState state) const;

  bool Equals(const Value& other) const;

  constexpr const Persistent<const Type>& value() const { return value_; }

 private:
  friend class PersistentValueHandle;
  template <size_t Size, size_t Align>
  friend class base_internal::AnyData;

  static constexpr uintptr_t kMetadata =
      base_internal::kStoredInline |
      (static_cast<uintptr_t>(kKind) << base_internal::kKindShift);

  explicit TypeValue(Persistent<const Type> value)
      : base_internal::InlineData(kMetadata), value_(std::move(value)) {}

  TypeValue(const TypeValue&) = default;
  TypeValue(TypeValue&&) = default;
  TypeValue& operator=(const TypeValue&) = default;
  TypeValue& operator=(TypeValue&&) = default;

  Persistent<const Type> value_;
};

CEL_INTERNAL_VALUE_DECL(TypeValue);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_TYPE_VALUE_H_
