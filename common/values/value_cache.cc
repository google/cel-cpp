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

#include "common/values/value_cache.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/types/type_cache.h"
#include "common/value.h"
#include "internal/no_destructor.h"

namespace cel {

namespace common_internal {
namespace {

class EmptyListValue final : public ListValueInterface {
 public:
  explicit EmptyListValue(ListType type) : type_(std::move(type)) {}

  std::string DebugString() const override { return "[]"; }

  bool IsEmpty() const override { return true; }

  size_t Size() const override { return 0; }

 private:
  NativeTypeId GetNativeTypeId() const noexcept override {
    return NativeTypeId::For<EmptyListValue>();
  }

  TypeView get_type() const override { return type_; }

  absl::StatusOr<ValueView> GetImpl(size_t, Value&) const override {
    // Not reachable, `Get` performs index checking.
    ABSL_UNREACHABLE();
  }

  const ListType type_;
};

class EmptyMapValueKeyIterator final : public ValueIterator {
 public:
  bool HasNext() override { return false; }

  absl::StatusOr<ValueView> Next(Value&) override {
    return absl::FailedPreconditionError(
        "ValueIterator::Next() called when "
        "ValueIterator::HasNext() returns false");
  }
};

class EmptyMapValue final : public MapValueInterface {
 public:
  explicit EmptyMapValue(MapType type) : type_(std::move(type)) {}

  std::string DebugString() const override { return "{}"; }

  bool IsEmpty() const override { return true; }

  size_t Size() const override { return 0; }

  absl::StatusOr<ListValue> ListKeys(TypeFactory& type_factory) const override {
    auto list_type = ProcessLocalTypeCache::Get()->FindListType(type_.key());
    if (!list_type.has_value()) {
      return absl::InternalError(
          "expected cached list type to be present in process local cache");
    }
    auto list_value =
        ProcessLocalValueCache::Get()->GetEmptyListValue(*list_type);
    if (!list_value.has_value()) {
      return absl::InternalError(
          "expected cached empty list value to be present in process local "
          "cache");
    }
    return ListValue(*list_value);
  }

  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator() const override {
    return std::make_unique<EmptyMapValueKeyIterator>();
  }

 private:
  NativeTypeId GetNativeTypeId() const noexcept override {
    return NativeTypeId::For<EmptyMapValue>();
  }

  TypeView get_type() const override { return type_; }

  absl::StatusOr<absl::optional<ValueView>> FindImpl(ValueView,
                                                     Value&) const override {
    return absl::nullopt;
  }

  absl::StatusOr<bool> HasImpl(ValueView) const override { return false; }

  const MapType type_;
};

class EmptyOptionalValue final : public OptionalValueInterface {
 public:
  explicit EmptyOptionalValue(OptionalType type) : type_(std::move(type)) {}

  bool HasValue() const override { return false; }

  ValueView Value(cel::Value& scratch) const override {
    scratch = ErrorValue(
        absl::FailedPreconditionError("optional.none() dereference"));
    return scratch;
  }

 private:
  friend struct NativeTypeTraits<EmptyOptionalValue>;

  TypeView get_type() const override { return type_; }

  const OptionalType type_;
};

}  // namespace

}  // namespace common_internal

template <>
struct NativeTypeTraits<common_internal::EmptyListValue> {
  static bool SkipDestructor(const common_internal::EmptyListValue&) {
    return true;
  }
};

template <>
struct NativeTypeTraits<common_internal::EmptyMapValue> {
  static bool SkipDestructor(const common_internal::EmptyMapValue&) {
    return true;
  }
};

template <>
struct NativeTypeTraits<common_internal::EmptyOptionalValue> {
  static bool SkipDestructor(const common_internal::EmptyOptionalValue&) {
    return true;
  }
};

namespace common_internal {

ErrorValueView GetDefaultErrorValue() {
  return ProcessLocalValueCache::Get()->GetDefaultErrorValue();
}

ListValueView GetEmptyDynListValue() {
  return ProcessLocalValueCache::Get()->GetEmptyDynListValue();
}

MapValueView GetEmptyDynDynMapValue() {
  return ProcessLocalValueCache::Get()->GetEmptyDynDynMapValue();
}

OptionalValueView GetEmptyDynOptionalValue() {
  return ProcessLocalValueCache::Get()->GetEmptyDynOptionalValue();
}

const ProcessLocalValueCache* ProcessLocalValueCache::Get() {
  static const internal::NoDestructor<ProcessLocalValueCache> instance;
  return &*instance;
}

ErrorValueView ProcessLocalValueCache::GetDefaultErrorValue() const {
  return default_error_value_;
}

absl::optional<ListValueView> ProcessLocalValueCache::GetEmptyListValue(
    ListTypeView type) const {
  if (auto list_value = list_values_.find(type);
      list_value != list_values_.end()) {
    return list_value->second;
  }
  return absl::nullopt;
}

ListValueView ProcessLocalValueCache::GetEmptyDynListValue() const {
  return *dyn_list_value_;
}

absl::optional<MapValueView> ProcessLocalValueCache::GetEmptyMapValue(
    MapTypeView type) const {
  if (auto map_value = map_values_.find(type); map_value != map_values_.end()) {
    return map_value->second;
  }
  return absl::nullopt;
}

MapValueView ProcessLocalValueCache::GetEmptyDynDynMapValue() const {
  return *dyn_dyn_map_value_;
}

absl::optional<OptionalValueView> ProcessLocalValueCache::GetEmptyOptionalValue(
    OptionalTypeView type) const {
  if (auto optional_value = optional_values_.find(type);
      optional_value != optional_values_.end()) {
    return optional_value->second;
  }
  return absl::nullopt;
}

OptionalValueView ProcessLocalValueCache::GetEmptyDynOptionalValue() const {
  return *dyn_optional_value_;
}

ProcessLocalValueCache::ProcessLocalValueCache()
    : default_error_value_(absl::UnknownError("unknown error")) {
  MemoryManagerRef memory_manager = MemoryManagerRef::Unmanaged();
  const auto& list_types = ProcessLocalTypeCache::Get()->ListTypes();
  list_values_.reserve(list_types.size());
  for (const auto& list_type : list_types) {
    auto inserted =
        list_values_
            .insert_or_assign(
                list_type, ListValue(memory_manager.MakeShared<EmptyListValue>(
                               ListType(list_type))))
            .second;
    ABSL_DCHECK(inserted);
  }
  const auto& map_types = ProcessLocalTypeCache::Get()->MapTypes();
  map_values_.reserve(map_types.size());
  for (const auto& map_type : map_types) {
    auto inserted =
        map_values_
            .insert_or_assign(map_type,
                              MapValue(memory_manager.MakeShared<EmptyMapValue>(
                                  MapType(map_type))))
            .second;
    ABSL_DCHECK(inserted);
  }
  const auto& optional_types = ProcessLocalTypeCache::Get()->OptionalTypes();
  optional_values_.reserve(optional_types.size());
  for (const auto& optional_type : optional_types) {
    auto inserted =
        optional_values_
            .insert_or_assign(
                optional_type,
                OptionalValue(memory_manager.MakeShared<EmptyOptionalValue>(
                    OptionalType(optional_type))))
            .second;
    ABSL_DCHECK(inserted);
  }
  dyn_list_value_ =
      GetEmptyListValue(ProcessLocalTypeCache::Get()->GetDynListType());
  ABSL_DCHECK(dyn_list_value_.has_value());
  dyn_dyn_map_value_ =
      GetEmptyMapValue(ProcessLocalTypeCache::Get()->GetDynDynMapType());
  ABSL_DCHECK(dyn_dyn_map_value_.has_value());
  dyn_optional_value_ =
      GetEmptyOptionalValue(ProcessLocalTypeCache::Get()->GetDynOptionalType());
  ABSL_DCHECK(dyn_optional_value_.has_value());
}

}  // namespace common_internal

}  // namespace cel
