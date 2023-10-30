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

#include "base/values/map_value.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "base/handle.h"
#include "base/internal/data.h"
#include "base/kind.h"
#include "base/memory.h"
#include "base/type.h"
#include "base/types/map_type.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "base/values/bool_value.h"
#include "base/values/int_value.h"
#include "base/values/list_value.h"
#include "base/values/uint_value.h"
#include "common/any.h"
#include "common/json.h"
#include "common/native_type.h"
#include "internal/status_macros.h"

namespace cel {

CEL_INTERNAL_VALUE_IMPL(MapValue);

absl::Status MapValue::CheckKey(const Value& value) {
  switch (value.kind()) {
    case ValueKind::kBool:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kInt:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUint:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kString:
      return absl::OkStatus();
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "Invalid map key type: '", ValueKindToString(value.kind()), "'"));
  }
}

#define CEL_INTERNAL_MAP_VALUE_DISPATCH(method, ...)                       \
  base_internal::Metadata::IsStoredInline(*this)                           \
      ? static_cast<const base_internal::LegacyMapValue&>(*this).method(   \
            __VA_ARGS__)                                                   \
      : static_cast<const base_internal::AbstractMapValue&>(*this).method( \
            __VA_ARGS__)

Handle<MapType> MapValue::type() const {
  return CEL_INTERNAL_MAP_VALUE_DISPATCH(type);
}

std::string MapValue::DebugString() const {
  return CEL_INTERNAL_MAP_VALUE_DISPATCH(DebugString);
}

absl::StatusOr<Any> MapValue::ConvertToAny(ValueFactory& value_factory) const {
  return CEL_INTERNAL_MAP_VALUE_DISPATCH(ConvertToAny, value_factory);
}

absl::StatusOr<Json> MapValue::ConvertToJson(
    ValueFactory& value_factory) const {
  return ConvertToJsonObject(value_factory);
}

absl::StatusOr<JsonObject> MapValue::ConvertToJsonObject(
    ValueFactory& value_factory) const {
  return CEL_INTERNAL_MAP_VALUE_DISPATCH(ConvertToJsonObject, value_factory);
}

absl::StatusOr<Handle<Value>> MapValue::ConvertToType(
    ValueFactory& value_factory, const Handle<Type>& type) const {
  switch (type->kind()) {
    case TypeKind::kMap:
      // At runtime, a map is a map. In C++ we currently attempt to propagate
      // the type if we know all the values have the same type, otherwise we use
      // dyn.
      if (!Value::IsRuntimeConvertible(*this->type()->key(),
                                       *type->As<MapType>().key()) ||
          !Value::IsRuntimeConvertible(*this->type()->value(),
                                       *type->As<MapType>().value())) {
        break;
      }
      return handle_from_this();
    case TypeKind::kType:
      return value_factory.CreateTypeValue(this->type());
    default:
      break;
  }
  return value_factory.CreateErrorValue(
      base_internal::TypeConversionError(*this->type(), *type));
}

size_t MapValue::size() const { return CEL_INTERNAL_MAP_VALUE_DISPATCH(size); }

bool MapValue::empty() const { return CEL_INTERNAL_MAP_VALUE_DISPATCH(empty); }

absl::StatusOr<Handle<Value>> MapValue::Get(ValueFactory& value_factory,
                                            const Handle<Value>& key) const {
  Handle<Value> value;
  bool ok;
  CEL_ASSIGN_OR_RETURN(std::tie(value, ok), Find(value_factory, key));
  if (!ok) {
    if (ABSL_PREDICT_FALSE(value)) {
      ABSL_DCHECK(value->Is<ErrorValue>() || value->Is<UnknownValue>());
      return value;
    }
    return value_factory.CreateErrorValue(
        base_internal::CreateNoSuchKeyError(*key));
  }
  // Returning an empty handle when the key was found is invalid.
  ABSL_DCHECK(value) << "empty handle returned for allegedly present key: "
                     << key->DebugString();
  return value;
}

absl::StatusOr<std::pair<Handle<Value>, bool>> MapValue::Find(
    ValueFactory& value_factory, const Handle<Value>& key) const {
  if (ABSL_PREDICT_FALSE(key->Is<ErrorValue>() || key->Is<UnknownValue>())) {
    return std::make_pair(key, false);
  }
  if (auto status = CheckKey(*key); ABSL_PREDICT_FALSE(!status.ok())) {
    return std::make_pair(value_factory.CreateErrorValue(std::move(status)),
                          false);
  }
  return CEL_INTERNAL_MAP_VALUE_DISPATCH(FindImpl, value_factory, key);
}

absl::StatusOr<Handle<Value>> MapValue::Has(ValueFactory& value_factory,
                                            const Handle<Value>& key) const {
  if (ABSL_PREDICT_FALSE(key->Is<ErrorValue>() || key->Is<UnknownValue>())) {
    return key;
  }
  if (auto status = CheckKey(*key); ABSL_PREDICT_FALSE(!status.ok())) {
    return value_factory.CreateErrorValue(std::move(status));
  }
  return CEL_INTERNAL_MAP_VALUE_DISPATCH(HasImpl, value_factory, key);
}

absl::StatusOr<Handle<ListValue>> MapValue::ListKeys(
    ValueFactory& value_factory) const {
  return CEL_INTERNAL_MAP_VALUE_DISPATCH(ListKeys, value_factory);
}

absl::StatusOr<absl::Nonnull<std::unique_ptr<MapValue::Iterator>>>
MapValue::NewIterator(ValueFactory& value_factory) const {
  return CEL_INTERNAL_MAP_VALUE_DISPATCH(NewIterator, value_factory);
}

absl::StatusOr<Handle<Value>> MapValue::Equals(ValueFactory& value_factory,
                                               const Value& other) const {
  return CEL_INTERNAL_MAP_VALUE_DISPATCH(Equals, value_factory, other);
}

NativeTypeId MapValue::GetNativeTypeId() const {
  return CEL_INTERNAL_MAP_VALUE_DISPATCH(GetNativeTypeId);
}

#undef CEL_INTERNAL_MAP_VALUE_DISPATCH

absl::StatusOr<Handle<Value>> GenericMapValueEquals(ValueFactory& value_factory,
                                                    const MapValue& lhs,
                                                    const MapValue& rhs) {
  if (&lhs == &rhs) {
    return value_factory.CreateBoolValue(true);
  }
  const auto lhs_size = lhs.size();
  if (lhs_size != rhs.size()) {
    return value_factory.CreateBoolValue(false);
  }
  if (lhs_size == 0) {
    return value_factory.CreateBoolValue(true);
  }
  CEL_ASSIGN_OR_RETURN(auto lhs_iterator, lhs.NewIterator(value_factory));
  for (size_t index = 0; index < lhs_size; ++index) {
    if (ABSL_PREDICT_FALSE(!lhs_iterator->HasNext())) {
      return absl::InternalError(
          "MapValue::Iterator has less entries than MapValue");
    }
    CEL_ASSIGN_OR_RETURN(auto lhs_key, lhs_iterator->Next());
    CEL_ASSIGN_OR_RETURN(auto rhs_value, rhs.Get(value_factory, lhs_key));
    if (rhs_value->Is<ErrorValue>() || rhs_value->Is<UnknownValue>()) {
      return value_factory.CreateBoolValue(false);
    }
    CEL_ASSIGN_OR_RETURN(auto lhs_value, lhs.Get(value_factory, lhs_key));
    CEL_ASSIGN_OR_RETURN(auto equal,
                         lhs_value->Equals(value_factory, *rhs_value));
    // TODO(uncreated-issue/32): fix this for homogeneous equality
    if (equal->Is<BoolValue>() && !equal->As<BoolValue>().NativeValue()) {
      return equal;
    }
  }
  if (ABSL_PREDICT_FALSE(lhs_iterator->HasNext())) {
    return absl::InternalError(
        "MapValue::Iterator has more entries than MapValue");
  }
  return value_factory.CreateBoolValue(true);
}

namespace base_internal {

absl::Status CreateNoSuchKeyError(absl::string_view key) {
  return absl::NotFoundError(absl::StrCat(kErrNoSuchKey, " : ", key));
}

absl::Status CreateNoSuchKeyError(const Value& value) {
  return CreateNoSuchKeyError(value.DebugString());
}

namespace {

class LegacyMapValueIterator final : public MapValue::Iterator {
 public:
  LegacyMapValueIterator(ValueFactory& value_factory, uintptr_t impl)
      : value_factory_(value_factory), impl_(impl) {}

  bool HasNext() override {
    if (ABSL_PREDICT_FALSE(!keys_iterator_.has_value())) {
      // First call.
      return !LegacyMapValueEmpty(impl_);
    }
    return (*keys_iterator_)->HasNext();
  }

  absl::StatusOr<Handle<Value>> Next() override {
    CEL_RETURN_IF_ERROR(OnNext());
    return (*keys_iterator_)->Next();
  }

 private:
  absl::Status OnNext() {
    if (ABSL_PREDICT_FALSE(!keys_iterator_.has_value())) {
      CEL_ASSIGN_OR_RETURN(keys_,
                           LegacyMapValueListKeys(impl_, value_factory_));
      CEL_ASSIGN_OR_RETURN(keys_iterator_, keys_->NewIterator(value_factory_));
      ABSL_CHECK((*keys_iterator_)->HasNext());  // Crash OK
    }
    return absl::OkStatus();
  }

  ValueFactory& value_factory_;
  const uintptr_t impl_;
  Handle<ListValue> keys_;
  absl::optional<absl::Nonnull<std::unique_ptr<ListValue::Iterator>>>
      keys_iterator_;
};

class AbstractMapValueIterator final : public MapValue::Iterator {
 public:
  AbstractMapValueIterator(ValueFactory& value_factory,
                           const AbstractMapValue* value)
      : value_factory_(value_factory), value_(value) {}

  bool HasNext() override {
    if (ABSL_PREDICT_FALSE(!keys_iterator_.has_value())) {
      // First call.
      return !value_->empty();
    }
    return (*keys_iterator_)->HasNext();
  }

  absl::StatusOr<Handle<Value>> Next() override {
    CEL_RETURN_IF_ERROR(OnNext());
    return (*keys_iterator_)->Next();
  }

 private:
  absl::Status OnNext() {
    if (ABSL_PREDICT_FALSE(!keys_iterator_.has_value())) {
      CEL_ASSIGN_OR_RETURN(keys_, value_->ListKeys(value_factory_));
      CEL_ASSIGN_OR_RETURN(keys_iterator_, keys_->NewIterator(value_factory_));
      ABSL_CHECK((*keys_iterator_)->HasNext());  // Crash OK
    }
    return absl::OkStatus();
  }

  ValueFactory& value_factory_;
  const AbstractMapValue* const value_;
  Handle<ListValue> keys_;
  absl::optional<absl::Nonnull<std::unique_ptr<ListValue::Iterator>>>
      keys_iterator_;
};

absl::StatusOr<Any> GenericMapValueConvertToAny(const MapValue& value,
                                                ValueFactory& value_factory) {
  CEL_ASSIGN_OR_RETURN(auto json, value.ConvertToJsonObject(value_factory));
  return JsonObjectToAny(json);
}

absl::StatusOr<JsonObject> GenericMapValueConvertToJsonObject(
    const MapValue& value, ValueFactory& value_factory) {
  JsonObjectBuilder builder;
  builder.reserve(value.size());
  CEL_ASSIGN_OR_RETURN(auto iterator, value.NewIterator(value_factory));
  while (iterator->HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto key, iterator->Next());
    absl::Cord key_json;
    if (key->Is<BoolValue>()) {
      if (key->As<BoolValue>().NativeValue()) {
        key_json = "true";
      } else {
        key_json = "false";
      }
    } else if (key->Is<IntValue>()) {
      key_json = absl::StrCat(key->As<IntValue>().NativeValue());
    } else if (key->Is<UintValue>()) {
      key_json = absl::StrCat(key->As<UintValue>().NativeValue());
    } else if (key->Is<StringValue>()) {
      key_json = key->As<StringValue>().ToCord();
    } else {
      return absl::FailedPreconditionError(absl::StrCat(
          "cannot serialize map with key of type ", key->type()->DebugString(),
          " to google.protobuf.Value"));
    }
    CEL_ASSIGN_OR_RETURN(auto value, value.Get(value_factory, key));
    CEL_ASSIGN_OR_RETURN(auto value_json, value->ConvertToJson(value_factory));
    if (ABSL_PREDICT_FALSE(
            !builder
                 .insert_or_assign(std::move(key_json), std::move(value_json))
                 .second)) {
      return absl::FailedPreconditionError(absl::StrCat(
          "cannot serialize map with duplicate keys ",
          key->type()->DebugString(), " to google.protobuf.Value"));
    }
  }
  return std::move(builder).Build();
}

}  // namespace

Handle<MapType> LegacyMapValue::type() const {
  return HandleFactory<MapType>::Make<LegacyMapType>();
}

std::string LegacyMapValue::DebugString() const { return "map"; }

absl::StatusOr<Any> LegacyMapValue::ConvertToAny(
    ValueFactory& value_factory) const {
  return GenericMapValueConvertToAny(*this, value_factory);
}

absl::StatusOr<JsonObject> LegacyMapValue::ConvertToJsonObject(
    ValueFactory& value_factory) const {
  return GenericMapValueConvertToJsonObject(*this, value_factory);
}

size_t LegacyMapValue::size() const { return LegacyMapValueSize(impl_); }

bool LegacyMapValue::empty() const { return LegacyMapValueEmpty(impl_); }

absl::StatusOr<std::pair<Handle<Value>, bool>> LegacyMapValue::FindImpl(
    ValueFactory& value_factory, const Handle<Value>& key) const {
  CEL_ASSIGN_OR_RETURN(auto value,
                       LegacyMapValueGet(impl_, value_factory, key));
  if (!value.has_value()) {
    return std::make_pair(Handle<Value>(), false);
  }
  return std::make_pair(std::move(*value), true);
}

absl::StatusOr<Handle<Value>> LegacyMapValue::HasImpl(
    ValueFactory& value_factory, const Handle<Value>& key) const {
  CEL_ASSIGN_OR_RETURN(auto has, LegacyMapValueHas(impl_, key));
  return value_factory.CreateBoolValue(has);
}

absl::StatusOr<Handle<ListValue>> LegacyMapValue::ListKeys(
    ValueFactory& value_factory) const {
  return LegacyMapValueListKeys(impl_, value_factory);
}

absl::StatusOr<absl::Nonnull<std::unique_ptr<MapValue::Iterator>>>
LegacyMapValue::NewIterator(ValueFactory& value_factory) const {
  return std::make_unique<LegacyMapValueIterator>(value_factory, impl_);
}

absl::StatusOr<Handle<Value>> LegacyMapValue::Equals(
    ValueFactory& value_factory, const Value& other) const {
  if (!other.Is<MapValue>()) {
    return value_factory.CreateBoolValue(false);
  }
  return GenericMapValueEquals(value_factory, *this, other.As<MapValue>());
}

AbstractMapValue::AbstractMapValue(Handle<MapType> type)
    : HeapData(kKind), type_(std::move(type)) {
  // Ensure `Value*` and `HeapData*` are not thunked.
  ABSL_ASSERT(reinterpret_cast<uintptr_t>(static_cast<Value*>(this)) ==
              reinterpret_cast<uintptr_t>(static_cast<HeapData*>(this)));
}

absl::StatusOr<Any> AbstractMapValue::ConvertToAny(
    ValueFactory& value_factory) const {
  return GenericMapValueConvertToAny(*this, value_factory);
}

absl::StatusOr<JsonObject> AbstractMapValue::ConvertToJsonObject(
    ValueFactory& value_factory) const {
  return GenericMapValueConvertToJsonObject(*this, value_factory);
}

absl::StatusOr<absl::Nonnull<std::unique_ptr<MapValue::Iterator>>>
AbstractMapValue::NewIterator(ValueFactory& value_factory) const {
  return std::make_unique<AbstractMapValueIterator>(value_factory, this);
}

absl::StatusOr<Handle<Value>> AbstractMapValue::Equals(
    ValueFactory& value_factory, const Value& other) const {
  if (!other.Is<MapValue>()) {
    return value_factory.CreateBoolValue(false);
  }
  return GenericMapValueEquals(value_factory, *this, other.As<MapValue>());
}

}  // namespace base_internal

}  // namespace cel
