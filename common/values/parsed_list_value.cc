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

#include <cstddef>
#include <memory>

#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/casting.h"
#include "common/value.h"
#include "common/values/values.h"
#include "internal/serialize.h"
#include "internal/status_macros.h"

namespace cel {

class ParsedListValueInterfaceIterator final : public ValueIterator {
 public:
  explicit ParsedListValueInterfaceIterator(
      const ParsedListValueInterface& interface, ValueManager& value_manager)
      : interface_(interface),
        value_manager_(value_manager),
        size_(interface_.Size()) {}

  bool HasNext() override { return index_ < size_; }

  absl::Status Next(ValueManager&, Value& result) override {
    if (ABSL_PREDICT_FALSE(index_ >= size_)) {
      return absl::FailedPreconditionError(
          "ValueIterator::Next() called when "
          "ValueIterator::HasNext() returns false");
    }
    return interface_.GetImpl(value_manager_, index_++, result);
  }

 private:
  const ParsedListValueInterface& interface_;
  ValueManager& value_manager_;
  const size_t size_;
  size_t index_ = 0;
};

absl::StatusOr<size_t> ParsedListValueInterface::GetSerializedSize(
    AnyToJsonConverter&) const {
  return absl::UnimplementedError(
      "preflighting serialization size is not implemented by this list");
}

absl::Status ParsedListValueInterface::SerializeTo(
    AnyToJsonConverter& value_manager, absl::Cord& value) const {
  CEL_ASSIGN_OR_RETURN(auto json, ConvertToJsonArray(value_manager));
  return internal::SerializeListValue(json, value);
}

absl::Status ParsedListValueInterface::Get(ValueManager& value_manager,
                                           size_t index, Value& result) const {
  if (ABSL_PREDICT_FALSE(index >= Size())) {
    return absl::InvalidArgumentError("index out of bounds");
  }
  return GetImpl(value_manager, index, result);
}

absl::Status ParsedListValueInterface::ForEach(ValueManager& value_manager,
                                               ForEachCallback callback) const {
  return ForEach(value_manager,
                 [callback](size_t, ValueView value) -> absl::StatusOr<bool> {
                   return callback(value);
                 });
}

absl::Status ParsedListValueInterface::ForEach(
    ValueManager& value_manager, ForEachWithIndexCallback callback) const {
  const size_t size = Size();
  for (size_t index = 0; index < size; ++index) {
    Value element;
    CEL_RETURN_IF_ERROR(GetImpl(value_manager, index, element));
    CEL_ASSIGN_OR_RETURN(auto ok, callback(index, element));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<absl::Nonnull<ValueIteratorPtr>>
ParsedListValueInterface::NewIterator(ValueManager& value_manager) const {
  return std::make_unique<ParsedListValueInterfaceIterator>(*this,
                                                            value_manager);
}

absl::Status ParsedListValueInterface::Equal(ValueManager& value_manager,
                                             ValueView other,
                                             Value& result) const {
  if (auto list_value = As<ListValueView>(other); list_value.has_value()) {
    return ListValueEqual(value_manager, *this, *list_value, result);
  }
  result = BoolValueView{false};
  return absl::OkStatus();
}

absl::Status ParsedListValueInterface::Contains(ValueManager& value_manager,
                                                ValueView other,
                                                Value& result) const {
  Value outcome = BoolValue(false);
  Value equal;
  CEL_RETURN_IF_ERROR(
      ForEach(value_manager,
              [&value_manager, other, &outcome,
               &equal](ValueView element) -> absl::StatusOr<bool> {
                CEL_RETURN_IF_ERROR(element.Equal(value_manager, other, equal));
                if (auto bool_result = As<BoolValue>(equal);
                    bool_result.has_value() && bool_result->NativeValue()) {
                  outcome = BoolValue(true);
                  return false;
                }
                return true;
              }));
  result = outcome;
  return absl::OkStatus();
}

}  // namespace cel
