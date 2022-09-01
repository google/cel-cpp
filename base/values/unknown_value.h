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

#ifndef THIRD_PARTY_CEL_CPP_BASE_VALUES_UNKNOWN_VALUE_H_
#define THIRD_PARTY_CEL_CPP_BASE_VALUES_UNKNOWN_VALUE_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/hash/hash.h"
#include "base/attribute_set.h"
#include "base/function_result_set.h"
#include "base/internal/unknown_set.h"
#include "base/types/unknown_type.h"
#include "base/value.h"

namespace cel {

class UnknownValue final : public Value, public base_internal::HeapData {
 public:
  static constexpr Kind kKind = UnknownType::kKind;

  static bool Is(const Value& value) { return value.kind() == kKind; }

  constexpr Kind kind() const { return kKind; }

  Persistent<const UnknownType> type() const { return UnknownType::Get(); }

  std::string DebugString() const;

  void HashValue(absl::HashState state) const;

  bool Equals(const Value& other) const;

  const AttributeSet& attribute_set() const {
    return impl_ != nullptr ? impl_->attributes
                            : base_internal::EmptyAttributeSet();
  }

  const FunctionResultSet& function_result_set() const {
    return impl_ != nullptr ? impl_->function_results
                            : base_internal::EmptyFunctionResultSet();
  }

 private:
  friend class cel::MemoryManager;
  friend class ValueFactory;
  friend std::shared_ptr<base_internal::UnknownSetImpl>
  interop_internal::GetUnknownValueImpl(
      const Persistent<const UnknownValue>& value);
  friend void interop_internal::SetUnknownValueImpl(
      Persistent<UnknownValue>& value,
      std::shared_ptr<base_internal::UnknownSetImpl> impl);

  UnknownValue() : UnknownValue(nullptr) {}

  explicit UnknownValue(std::shared_ptr<base_internal::UnknownSetImpl> impl);

  UnknownValue(AttributeSet attribute_set,
               FunctionResultSet function_result_set)
      : UnknownValue(std::make_shared<base_internal::UnknownSetImpl>(
            std::move(attribute_set), std::move(function_result_set))) {}

  std::shared_ptr<base_internal::UnknownSetImpl> impl_;
};

CEL_INTERNAL_VALUE_DECL(UnknownValue);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_UNKNOWN_VALUE_H_
