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
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "common/value_manager.h"
#include "internal/status_macros.h"

namespace cel {

namespace {

class ListValueImpl final : public ParsedListValueInterface {
 public:
  explicit ListValueImpl(std::vector<Value>&& elements)
      : elements_(std::move(elements)) {}

  std::string DebugString() const override {
    return absl::StrCat(
        "[", absl::StrJoin(elements_, ", ", absl::StreamFormatter()), "]");
  }

  bool IsEmpty() const override { return elements_.empty(); }

  size_t Size() const override { return elements_.size(); }

  absl::StatusOr<JsonArray> ConvertToJsonArray(
      AnyToJsonConverter& converter) const override {
    JsonArrayBuilder builder;
    builder.reserve(Size());
    for (const auto& element : elements_) {
      CEL_ASSIGN_OR_RETURN(auto json_element, element.ConvertToJson(converter));
      builder.push_back(std::move(json_element));
    }
    return std::move(builder).Build();
  }

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachCallback callback) const override {
    for (const auto& element : elements_) {
      CEL_ASSIGN_OR_RETURN(auto ok, callback(element));
      if (!ok) {
        break;
      }
    }
    return absl::OkStatus();
  }

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachWithIndexCallback callback) const override {
    for (size_t i = 0; i < elements_.size(); ++i) {
      CEL_ASSIGN_OR_RETURN(auto ok, callback(i, elements_[i]));
      if (!ok) {
        break;
      }
    }
    return absl::OkStatus();
  }

  absl::Status Contains(ValueManager& value_manager, const Value& other,
                        Value& result) const override {
    for (size_t i = 0; i < elements_.size(); ++i) {
      CEL_RETURN_IF_ERROR(elements_[i].Equal(value_manager, other, result));
      if (auto bool_result = As<BoolValue>(result);
          bool_result.has_value() && bool_result->NativeValue()) {
        return absl::OkStatus();
      }
    }
    result = BoolValue(false);
    return absl::OkStatus();
  }

 private:
  absl::Status GetImpl(ValueManager&, size_t index,
                       Value& result) const override {
    result = elements_[index];
    return absl::OkStatus();
  }

  NativeTypeId GetNativeTypeId() const noexcept override {
    return NativeTypeId::For<ListValueImpl>();
  }

  const ListType type_;
  const std::vector<Value> elements_;
};

class ListValueBuilderImpl final : public ListValueBuilder {
 public:
  explicit ListValueBuilderImpl(MemoryManagerRef memory_manager)
      : memory_manager_(memory_manager) {}

  ListValueBuilderImpl(const ListValueBuilderImpl&) = delete;
  ListValueBuilderImpl(ListValueBuilderImpl&&) = delete;
  ListValueBuilderImpl& operator=(const ListValueBuilderImpl&) = delete;
  ListValueBuilderImpl& operator=(ListValueBuilderImpl&&) = delete;

  absl::Status Add(Value value) override {
    if (value.Is<ErrorValue>()) {
      return std::move(value).Get<ErrorValue>().NativeValue();
    }
    elements_.push_back(std::move(value));
    return absl::OkStatus();
  }

  bool IsEmpty() const override { return elements_.empty(); }

  size_t Size() const override { return elements_.size(); }

  void Reserve(size_t capacity) override { elements_.reserve(capacity); }

  ListValue Build() && override {
    return ParsedListValue(
        memory_manager_.MakeShared<ListValueImpl>(std::move(elements_)));
  }

 private:
  MemoryManagerRef memory_manager_;
  std::vector<Value> elements_;
};

}  // namespace

absl::StatusOr<absl::Nonnull<ListValueBuilderPtr>>
TypeReflector::NewListValueBuilder(ValueFactory& value_factory,
                                   const ListType& type) const {
  auto memory_manager = value_factory.GetMemoryManager();
  return std::make_unique<ListValueBuilderImpl>(memory_manager);
}

namespace common_internal {

absl::StatusOr<absl::Nonnull<ListValueBuilderPtr>>
LegacyTypeReflector::NewListValueBuilder(ValueFactory& value_factory,
                                         const ListType& type) const {
  auto memory_manager = value_factory.GetMemoryManager();
  if (memory_manager.memory_management() == MemoryManagement::kPooling) {
    auto status_or_builder = NewLegacyListValueBuilder(value_factory, type);
    if (status_or_builder.ok()) {
      return std::move(status_or_builder).value();
    }
    if (!absl::IsUnimplemented(status_or_builder.status())) {
      return status_or_builder;
    }
  }
  return TypeReflector::NewListValueBuilder(value_factory, type);
}
}  // namespace common_internal

}  // namespace cel
