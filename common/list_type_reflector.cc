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
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/call_once.h"
#include "absl/base/macros.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/type_kind.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "common/value_manager.h"
#include "internal/dynamic_loader.h"
#include "internal/status_macros.h"

namespace cel {

namespace {

template <typename T>
class TypedListValue final : public ParsedListValueInterface {
 public:
  TypedListValue(ListType type, std::vector<T>&& elements)
      : type_(std::move(type)), elements_(std::move(elements)) {}

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

  absl::StatusOr<ValueView> Contains(
      ValueManager& value_manager, ValueView other,
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const override {
    ValueView outcome = BoolValueView{false};
    for (size_t i = 0; i < elements_.size(); ++i) {
      CEL_ASSIGN_OR_RETURN(auto result,
                           elements_[i].Equal(value_manager, other, scratch));
      if (auto bool_result = As<BoolValueView>(result);
          bool_result.has_value() && bool_result->NativeValue()) {
        outcome = *bool_result;
        break;
      }
    }
    return outcome;
  }

 protected:
  Type GetTypeImpl(TypeManager&) const override { return type_; }

 private:
  absl::StatusOr<ValueView> GetImpl(ValueManager&, size_t index,
                                    Value&) const override {
    return elements_[index];
  }

  NativeTypeId GetNativeTypeId() const noexcept override {
    return NativeTypeId::For<TypedListValue<T>>();
  }

  const ListType type_;
  const std::vector<T> elements_;
};

template <typename T>
class ListValueBuilderImpl final : public ListValueBuilder {
 public:
  using element_type = std::decay_t<decltype(std::declval<T>().GetType(
      std::declval<TypeManager&>()))>;
  using element_view_type = typename element_type::view_alternative_type;

  static_assert(common_internal::IsValueAlternativeV<T> ||
                    std::is_same_v<T, ListValue> ||
                    std::is_same_v<T, MapValue> ||
                    std::is_same_v<T, StructValue>,
                "T must be Value or one of the Value alternatives");

  ListValueBuilderImpl(MemoryManagerRef memory_manager, ListType type)
      : memory_manager_(memory_manager), type_(std::move(type)) {}

  ListValueBuilderImpl(const ListValueBuilderImpl&) = delete;
  ListValueBuilderImpl(ListValueBuilderImpl&&) = delete;
  ListValueBuilderImpl& operator=(const ListValueBuilderImpl&) = delete;
  ListValueBuilderImpl& operator=(ListValueBuilderImpl&&) = delete;

  absl::Status Add(Value value) override {
    if (value.Is<ErrorValue>()) {
      return value.As<ErrorValue>().NativeValue();
    }
    return Add(Cast<T>(std::move(value)));
  }

  absl::Status Add(T value) {
    elements_.push_back(std::move(value));
    return absl::OkStatus();
  }

  bool IsEmpty() const override { return elements_.empty(); }

  size_t Size() const override { return elements_.size(); }

  void Reserve(size_t capacity) override { elements_.reserve(capacity); }

  ListValue Build() && override {
    return ParsedListValue(
        memory_manager_.template MakeShared<TypedListValue<T>>(
            std::move(type_), std::move(elements_)));
  }

 private:
  MemoryManagerRef memory_manager_;
  ListType type_;
  std::vector<T> elements_;
};

template <>
class ListValueBuilderImpl<Value> final : public ListValueBuilder {
 public:
  ListValueBuilderImpl(MemoryManagerRef memory_manager, ListType type)
      : memory_manager_(memory_manager), type_(std::move(type)) {}

  ListValueBuilderImpl(const ListValueBuilderImpl&) = delete;
  ListValueBuilderImpl(ListValueBuilderImpl&&) = delete;
  ListValueBuilderImpl& operator=(const ListValueBuilderImpl&) = delete;
  ListValueBuilderImpl& operator=(ListValueBuilderImpl&&) = delete;

  absl::Status Add(Value value) override {
    if (value.Is<ErrorValue>()) {
      return value.As<ErrorValue>().NativeValue();
    }
    elements_.push_back(std::move(value));
    return absl::OkStatus();
  }

  bool IsEmpty() const override { return elements_.empty(); }

  size_t Size() const override { return elements_.size(); }

  void Reserve(size_t capacity) override { elements_.reserve(capacity); }

  ListValue Build() && override {
    return ParsedListValue(memory_manager_.MakeShared<TypedListValue<Value>>(
        std::move(type_), std::move(elements_)));
  }

 private:
  MemoryManagerRef memory_manager_;
  ListType type_;
  std::vector<Value> elements_;
};

using LegacyTypeReflector_NewListValueBuilder =
    absl::StatusOr<Unique<ListValueBuilder>> (*)(ValueFactory&, ListTypeView);

ABSL_CONST_INIT struct {
  absl::once_flag init_once;
  LegacyTypeReflector_NewListValueBuilder new_list_value_builder = nullptr;
} legacy_type_reflector_vtable;

#if ABSL_HAVE_ATTRIBUTE_WEAK
extern "C" ABSL_ATTRIBUTE_WEAK absl::StatusOr<Unique<ListValueBuilder>>
cel_common_internal_LegacyTypeReflector_NewListValueBuilder(
    ValueFactory& value_factory, ListTypeView type);
#endif

void InitializeLegacyTypeReflector() {
  absl::call_once(legacy_type_reflector_vtable.init_once, []() -> void {
#if ABSL_HAVE_ATTRIBUTE_WEAK
    legacy_type_reflector_vtable.new_list_value_builder =
        cel_common_internal_LegacyTypeReflector_NewListValueBuilder;
#else
    internal::DynamicLoader dynamic_loader;
    if (auto new_list_value_builder = dynamic_loader.FindSymbol(
            "cel_common_internal_LegacyTypeReflector_NewListValueBuilder");
        new_list_value_builder) {
      legacy_type_reflector_vtable.new_list_value_builder =
          *new_list_value_builder;
    }
#endif
  });
}

}  // namespace

absl::StatusOr<Unique<ListValueBuilder>> TypeReflector::NewListValueBuilder(
    ValueFactory& value_factory, ListTypeView type) const {
  InitializeLegacyTypeReflector();
  auto memory_manager = value_factory.GetMemoryManager();
  if (memory_manager.memory_management() == MemoryManagement::kPooling &&
      legacy_type_reflector_vtable.new_list_value_builder != nullptr) {
    auto status_or_builder =
        (*legacy_type_reflector_vtable.new_list_value_builder)(value_factory,
                                                               type);
    if (status_or_builder.ok()) {
      return std::move(status_or_builder).value();
    }
    if (!absl::IsUnimplemented(status_or_builder.status())) {
      return status_or_builder;
    }
  }
  switch (type.element().kind()) {
    case TypeKind::kBool:
      return memory_manager.MakeUnique<ListValueBuilderImpl<BoolValue>>(
          memory_manager, ListType(type));
    case TypeKind::kBytes:
      return memory_manager.MakeUnique<ListValueBuilderImpl<BytesValue>>(
          memory_manager, ListType(type));
    case TypeKind::kDouble:
      return memory_manager.MakeUnique<ListValueBuilderImpl<DoubleValue>>(
          memory_manager, ListType(type));
    case TypeKind::kDuration:
      return memory_manager.MakeUnique<ListValueBuilderImpl<DurationValue>>(
          memory_manager, ListType(type));
    case TypeKind::kInt:
      return memory_manager.MakeUnique<ListValueBuilderImpl<IntValue>>(
          memory_manager, ListType(type));
    case TypeKind::kList:
      return memory_manager.MakeUnique<ListValueBuilderImpl<ListValue>>(
          memory_manager, ListType(type));
    case TypeKind::kMap:
      return memory_manager.MakeUnique<ListValueBuilderImpl<MapValue>>(
          memory_manager, ListType(type));
    case TypeKind::kNull:
      return memory_manager.MakeUnique<ListValueBuilderImpl<NullValue>>(
          memory_manager, ListType(type));
    case TypeKind::kOpaque:
      return memory_manager.MakeUnique<ListValueBuilderImpl<OpaqueValue>>(
          memory_manager, ListType(type));
    case TypeKind::kString:
      return memory_manager.MakeUnique<ListValueBuilderImpl<StringValue>>(
          memory_manager, ListType(type));
    case TypeKind::kTimestamp:
      return memory_manager.MakeUnique<ListValueBuilderImpl<TimestampValue>>(
          memory_manager, ListType(type));
    case TypeKind::kType:
      return memory_manager.MakeUnique<ListValueBuilderImpl<TypeValue>>(
          memory_manager, ListType(type));
    case TypeKind::kUint:
      return memory_manager.MakeUnique<ListValueBuilderImpl<UintValue>>(
          memory_manager, ListType(type));
    case TypeKind::kStruct:
      return memory_manager.MakeUnique<ListValueBuilderImpl<StructValue>>(
          memory_manager, ListType(type));
    case TypeKind::kDyn:
      return memory_manager.MakeUnique<ListValueBuilderImpl<Value>>(
          memory_manager, ListType(type));
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "invalid list element type: ", type.element().DebugString()));
  }
}

}  // namespace cel
