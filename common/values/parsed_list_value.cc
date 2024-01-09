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
#include "common/value.h"
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

  absl::StatusOr<ValueView> Next(Value& scratch) override {
    if (ABSL_PREDICT_FALSE(index_ >= size_)) {
      return absl::FailedPreconditionError(
          "ValueIterator::Next() called when "
          "ValueIterator::HasNext() returns false");
    }
    return interface_.GetImpl(value_manager_, index_++, scratch);
  }

 private:
  const ParsedListValueInterface& interface_;
  ValueManager& value_manager_;
  const size_t size_;
  size_t index_ = 0;
};

absl::StatusOr<size_t> ParsedListValueInterface::GetSerializedSize() const {
  return absl::UnimplementedError(
      "preflighting serialization size is not implemented by this list");
}

absl::Status ParsedListValueInterface::SerializeTo(absl::Cord& value) const {
  CEL_ASSIGN_OR_RETURN(auto json, ConvertToJsonArray());
  return internal::SerializeListValue(json, value);
}

absl::StatusOr<ValueView> ParsedListValueInterface::Get(
    ValueManager& value_manager, size_t index, Value& scratch) const {
  if (ABSL_PREDICT_FALSE(index >= Size())) {
    return absl::InvalidArgumentError("index out of bounds");
  }
  return GetImpl(value_manager, index, scratch);
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
    Value scratch;
    CEL_ASSIGN_OR_RETURN(auto element, GetImpl(value_manager, index, scratch));
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

}  // namespace cel
