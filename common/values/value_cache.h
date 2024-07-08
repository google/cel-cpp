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

// IWYU pragma: private

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_VALUE_CACHE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_VALUE_CACHE_H_

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "common/json.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/types/optional_type.h"
#include "common/types/type_cache.h"
#include "common/value.h"

namespace cel {

namespace common_internal {

using ListValueCacheMap = absl::flat_hash_map<ListTypeView, ParsedListValue>;
using MapValueCacheMap = absl::flat_hash_map<MapTypeView, ParsedMapValue>;
using OptionalValueCacheMap =
    absl::flat_hash_map<OptionalTypeView, OptionalValue>;

class ProcessLocalValueCache final {
 public:
  static absl::Nonnull<const ProcessLocalValueCache*> Get();

  ErrorValueView GetDefaultErrorValue() const;

  absl::optional<ParsedListValueView> GetEmptyListValue(
      ListTypeView type) const;

  ParsedListValueView GetEmptyDynListValue() const;

  absl::optional<ParsedMapValueView> GetEmptyMapValue(MapTypeView type) const;

  ParsedMapValueView GetEmptyDynDynMapValue() const;

  ParsedMapValueView GetEmptyStringDynMapValue() const;

  absl::optional<OptionalValueView> GetEmptyOptionalValue(
      OptionalTypeView type) const;

  OptionalValueView GetEmptyDynOptionalValue() const;

 private:
  friend class absl::NoDestructor<ProcessLocalValueCache>;

  ProcessLocalValueCache();

  const ErrorValue default_error_value_;
  ListValueCacheMap list_values_;
  MapValueCacheMap map_values_;
  OptionalValueCacheMap optional_values_;
  absl::optional<ParsedListValueView> dyn_list_value_;
  absl::optional<ParsedMapValueView> dyn_dyn_map_value_;
  absl::optional<ParsedMapValueView> string_dyn_map_value_;
  absl::optional<OptionalValueView> dyn_optional_value_;
};

class EmptyListValue final : public ParsedListValueInterface {
 public:
  explicit EmptyListValue(ListType type) : type_(std::move(type)) {}

  std::string DebugString() const override { return "[]"; }

  bool IsEmpty() const override { return true; }

  size_t Size() const override { return 0; }

  absl::StatusOr<JsonArray> ConvertToJsonArray(
      AnyToJsonConverter&) const override {
    return JsonArray();
  }

  ListTypeView GetType() const { return type_; }

 private:
  NativeTypeId GetNativeTypeId() const noexcept override {
    return NativeTypeId::For<EmptyListValue>();
  }

  Type GetTypeImpl(TypeManager&) const override { return type_; }

  absl::Status GetImpl(ValueManager&, size_t, Value&) const override {
    // Not reachable, `Get` performs index checking.
    ABSL_UNREACHABLE();
  }

  const ListType type_;
};

class EmptyMapValueKeyIterator final : public ValueIterator {
 public:
  bool HasNext() override { return false; }

  absl::Status Next(ValueManager&, Value&) override {
    return absl::FailedPreconditionError(
        "ValueIterator::Next() called when "
        "ValueIterator::HasNext() returns false");
  }
};

class EmptyMapValue final : public ParsedMapValueInterface {
 public:
  explicit EmptyMapValue(MapType type) : type_(std::move(type)) {}

  std::string DebugString() const override { return "{}"; }

  bool IsEmpty() const override { return true; }

  size_t Size() const override { return 0; }

  absl::Status ListKeys(ValueManager&, ListValue& result) const override {
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
    result = ListValue(*list_value);
    return absl::OkStatus();
  }

  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueManager&) const override {
    return std::make_unique<EmptyMapValueKeyIterator>();
  }

  absl::StatusOr<JsonObject> ConvertToJsonObject(
      AnyToJsonConverter&) const override {
    return JsonObject();
  }

  MapTypeView GetType() const { return type_; }

 private:
  NativeTypeId GetNativeTypeId() const noexcept override {
    return NativeTypeId::For<EmptyMapValue>();
  }

  Type GetTypeImpl(TypeManager&) const override { return type_; }

  absl::StatusOr<bool> FindImpl(ValueManager&, const Value&,
                                Value&) const override {
    return false;
  }

  absl::StatusOr<bool> HasImpl(ValueManager&, const Value&) const override {
    return false;
  }

  const MapType type_;
};

class EmptyOptionalValue final : public OptionalValueInterface {
 public:
  explicit EmptyOptionalValue(OptionalType type) : type_(std::move(type)) {}

  bool HasValue() const override { return false; }

  void Value(cel::Value& result) const override {
    result = ErrorValue(
        absl::FailedPreconditionError("optional.none() dereference"));
  }

  OptionalTypeView GetType() const { return type_; }

 private:
  friend struct NativeTypeTraits<EmptyOptionalValue>;

  Type GetTypeImpl(TypeManager&) const override { return type_; }

  const OptionalType type_;
};

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

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_VALUE_CACHE_H_
