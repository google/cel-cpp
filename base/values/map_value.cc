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
#include <string>
#include <utility>

#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "base/handle.h"
#include "base/internal/data.h"
#include "base/types/map_type.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "base/values/list_value.h"
#include "internal/rtti.h"
#include "internal/status_macros.h"

namespace cel {

CEL_INTERNAL_VALUE_IMPL(MapValue);

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

size_t MapValue::size() const { return CEL_INTERNAL_MAP_VALUE_DISPATCH(size); }

bool MapValue::empty() const { return CEL_INTERNAL_MAP_VALUE_DISPATCH(empty); }

absl::StatusOr<absl::optional<Handle<Value>>> MapValue::Get(
    const GetContext& context, const Handle<Value>& key) const {
  return CEL_INTERNAL_MAP_VALUE_DISPATCH(Get, context, key);
}

absl::StatusOr<bool> MapValue::Has(const HasContext& context,
                                   const Handle<Value>& key) const {
  return CEL_INTERNAL_MAP_VALUE_DISPATCH(Has, context, key);
}

absl::StatusOr<Handle<ListValue>> MapValue::ListKeys(
    const ListKeysContext& context) const {
  return CEL_INTERNAL_MAP_VALUE_DISPATCH(ListKeys, context);
}

absl::StatusOr<UniqueRef<MapValue::Iterator>> MapValue::NewIterator(
    ValueFactory& value_factory) const {
  return CEL_INTERNAL_MAP_VALUE_DISPATCH(NewIterator, value_factory);
}

internal::TypeInfo MapValue::TypeId() const {
  return CEL_INTERNAL_MAP_VALUE_DISPATCH(TypeId);
}

#undef CEL_INTERNAL_MAP_VALUE_DISPATCH

absl::StatusOr<Handle<Value>> MapValue::Iterator::NextKey() {
  CEL_ASSIGN_OR_RETURN(auto entry, Next());
  return std::move(entry.key);
}

absl::StatusOr<Handle<Value>> MapValue::Iterator::NextValue() {
  CEL_ASSIGN_OR_RETURN(auto entry, Next());
  return std::move(entry.value);
}

namespace base_internal {

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

  absl::StatusOr<Entry> Next() override {
    CEL_RETURN_IF_ERROR(OnNext());
    CEL_ASSIGN_OR_RETURN(auto key, (*keys_iterator_)->NextValue());
    CEL_ASSIGN_OR_RETURN(auto value,
                         LegacyMapValueGet(impl_, value_factory_, key));
    if (ABSL_PREDICT_FALSE(!value.has_value())) {
      // Something is seriously wrong. The list of keys from the map is not
      // consistent with what the map believes is set.
      return absl::InternalError(
          "inconsistency between list of map keys and map");
    }
    return Entry(std::move(key), std::move(value).value());
  }

  absl::StatusOr<Handle<Value>> NextKey() override {
    CEL_RETURN_IF_ERROR(OnNext());
    CEL_ASSIGN_OR_RETURN(auto key, (*keys_iterator_)->NextValue());
    return key;
  }

  absl::StatusOr<Handle<Value>> NextValue() override {
    CEL_RETURN_IF_ERROR(OnNext());
    CEL_ASSIGN_OR_RETURN(auto key, (*keys_iterator_)->NextValue());
    CEL_ASSIGN_OR_RETURN(auto value,
                         LegacyMapValueGet(impl_, value_factory_, key));
    if (ABSL_PREDICT_FALSE(!value.has_value())) {
      // Something is seriously wrong. The list of keys from the map is not
      // consistent with what the map believes is set.
      return absl::InternalError(
          "inconsistency between list of map keys and map");
    }
    return std::move(value).value();
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
  absl::optional<UniqueRef<ListValue::Iterator>> keys_iterator_;
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

  absl::StatusOr<Entry> Next() override {
    CEL_RETURN_IF_ERROR(OnNext());
    CEL_ASSIGN_OR_RETURN(auto key, (*keys_iterator_)->NextValue());
    CEL_ASSIGN_OR_RETURN(
        auto value, value_->Get(MapValue::GetContext(value_factory_), key));
    if (ABSL_PREDICT_FALSE(!value.has_value())) {
      // Something is seriously wrong. The list of keys from the map is not
      // consistent with what the map believes is set.
      return absl::InternalError(
          "inconsistency between list of map keys and map");
    }
    return Entry(std::move(key), std::move(value).value());
  }

  absl::StatusOr<Handle<Value>> NextKey() override {
    CEL_RETURN_IF_ERROR(OnNext());
    CEL_ASSIGN_OR_RETURN(auto key, (*keys_iterator_)->NextValue());
    return key;
  }

 private:
  absl::Status OnNext() {
    if (ABSL_PREDICT_FALSE(!keys_iterator_.has_value())) {
      CEL_ASSIGN_OR_RETURN(
          keys_, value_->ListKeys(MapValue::ListKeysContext(value_factory_)));
      CEL_ASSIGN_OR_RETURN(keys_iterator_, keys_->NewIterator(value_factory_));
      ABSL_CHECK((*keys_iterator_)->HasNext());  // Crash OK
    }
    return absl::OkStatus();
  }

  ValueFactory& value_factory_;
  const AbstractMapValue* const value_;
  Handle<ListValue> keys_;
  absl::optional<UniqueRef<ListValue::Iterator>> keys_iterator_;
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
    CEL_ASSIGN_OR_RETURN(auto entry, iterator->Next());
    absl::Cord key_json;
    if (entry.key->Is<BoolValue>()) {
      if (entry.key->As<BoolValue>().value()) {
        key_json = "true";
      } else {
        key_json = "false";
      }
    } else if (entry.key->Is<IntValue>()) {
      key_json = absl::StrCat(entry.key->As<IntValue>().value());
    } else if (entry.key->Is<UintValue>()) {
      key_json = absl::StrCat(entry.key->As<UintValue>().value());
    } else if (entry.key->Is<StringValue>()) {
      key_json = entry.key->As<StringValue>().ToCord();
    } else {
      return absl::FailedPreconditionError(absl::StrCat(
          "cannot serialize map with key of type ",
          entry.key->type()->DebugString(), " to google.protobuf.Value"));
    }
    CEL_ASSIGN_OR_RETURN(auto value_json,
                         entry.value->ConvertToJson(value_factory));
    if (ABSL_PREDICT_FALSE(
            !builder
                 .insert_or_assign(std::move(key_json), std::move(value_json))
                 .second)) {
      return absl::FailedPreconditionError(absl::StrCat(
          "cannot serialize map with duplicate keys ",
          entry.key->type()->DebugString(), " to google.protobuf.Value"));
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

absl::StatusOr<absl::optional<Handle<Value>>> LegacyMapValue::Get(
    const GetContext& context, const Handle<Value>& key) const {
  return LegacyMapValueGet(impl_, context.value_factory(), key);
}

absl::StatusOr<bool> LegacyMapValue::Has(const HasContext& context,
                                         const Handle<Value>& key) const {
  static_cast<void>(context);
  return LegacyMapValueHas(impl_, key);
}

absl::StatusOr<Handle<ListValue>> LegacyMapValue::ListKeys(
    const ListKeysContext& context) const {
  return LegacyMapValueListKeys(impl_, context.value_factory());
}

absl::StatusOr<UniqueRef<MapValue::Iterator>> LegacyMapValue::NewIterator(
    ValueFactory& value_factory) const {
  return MakeUnique<LegacyMapValueIterator>(value_factory.memory_manager(),
                                            value_factory, impl_);
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

absl::StatusOr<UniqueRef<MapValue::Iterator>> AbstractMapValue::NewIterator(
    ValueFactory& value_factory) const {
  return MakeUnique<AbstractMapValueIterator>(value_factory.memory_manager(),
                                              value_factory, this);
}

}  // namespace base_internal

}  // namespace cel
