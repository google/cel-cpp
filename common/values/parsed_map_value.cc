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
#include <tuple>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/casting.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "common/values/values.h"
#include "internal/serialize.h"
#include "internal/status_macros.h"

namespace cel {

namespace {

absl::Status NoSuchKeyError(const Value& key) {
  return absl::NotFoundError(
      absl::StrCat("Key not found in map : ", key.DebugString()));
}

absl::Status InvalidMapKeyTypeError(ValueKind kind) {
  return absl::InvalidArgumentError(
      absl::StrCat("Invalid map key type: '", ValueKindToString(kind), "'"));
}

}  // namespace

absl::StatusOr<size_t> ParsedMapValueInterface::GetSerializedSize(
    AnyToJsonConverter&) const {
  return absl::UnimplementedError(
      "preflighting serialization size is not implemented by this map");
}

absl::Status ParsedMapValueInterface::SerializeTo(
    AnyToJsonConverter& value_manager, absl::Cord& value) const {
  CEL_ASSIGN_OR_RETURN(auto json, ConvertToJsonObject(value_manager));
  return internal::SerializeStruct(json, value);
}

absl::Status ParsedMapValueInterface::Get(ValueManager& value_manager,
                                          const Value& key,
                                          Value& result) const {
  CEL_ASSIGN_OR_RETURN(bool ok, Find(value_manager, key, result));
  if (ABSL_PREDICT_FALSE(!ok)) {
    switch (result.kind()) {
      case ValueKind::kError:
        ABSL_FALLTHROUGH_INTENDED;
      case ValueKind::kUnknown:
        break;
      default:
        return NoSuchKeyError(key);
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<bool> ParsedMapValueInterface::Find(ValueManager& value_manager,
                                                   const Value& key,
                                                   Value& result) const {
  switch (key.kind()) {
    case ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUnknown:
      result = Value(key);
      return false;
    case ValueKind::kBool:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kInt:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUint:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kString:
      break;
    default:
      return InvalidMapKeyTypeError(key.kind());
  }
  CEL_ASSIGN_OR_RETURN(auto ok, FindImpl(value_manager, key, result));
  if (ok) {
    return true;
  }
  result = NullValue{};
  return false;
}

absl::Status ParsedMapValueInterface::Has(ValueManager& value_manager,
                                          const Value& key,
                                          Value& result) const {
  switch (key.kind()) {
    case ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUnknown:
      result = Value{key};
      return absl::OkStatus();
    case ValueKind::kBool:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kInt:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUint:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kString:
      break;
    default:
      return InvalidMapKeyTypeError(key.kind());
  }
  CEL_ASSIGN_OR_RETURN(auto has, HasImpl(value_manager, key));
  result = BoolValue(has);
  return absl::OkStatus();
}

absl::Status ParsedMapValueInterface::ForEach(ValueManager& value_manager,
                                              ForEachCallback callback) const {
  CEL_ASSIGN_OR_RETURN(auto iterator, NewIterator(value_manager));
  while (iterator->HasNext()) {
    Value key;
    Value value;
    CEL_RETURN_IF_ERROR(iterator->Next(value_manager, key));
    CEL_RETURN_IF_ERROR(Get(value_manager, key, value));
    CEL_ASSIGN_OR_RETURN(auto ok, callback(key, value));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

absl::Status ParsedMapValueInterface::Equal(ValueManager& value_manager,
                                            const Value& other,
                                            Value& result) const {
  if (auto list_value = As<MapValue>(other); list_value.has_value()) {
    return MapValueEqual(value_manager, *this, *list_value, result);
  }
  result = BoolValue{false};
  return absl::OkStatus();
}

}  // namespace cel
