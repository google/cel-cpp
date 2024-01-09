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

absl::Status NoSuchKeyError(ValueView key) {
  return absl::NotFoundError(
      absl::StrCat("Key not found in map : ", key.DebugString()));
}

absl::Status InvalidMapKeyTypeError(ValueKind kind) {
  return absl::InvalidArgumentError(
      absl::StrCat("Invalid map key type: '", ValueKindToString(kind), "'"));
}

}  // namespace

absl::StatusOr<size_t> ParsedMapValueInterface::GetSerializedSize() const {
  return absl::UnimplementedError(
      "preflighting serialization size is not implemented by this map");
}

absl::Status ParsedMapValueInterface::SerializeTo(absl::Cord& value) const {
  CEL_ASSIGN_OR_RETURN(auto json, ConvertToJsonObject());
  return internal::SerializeStruct(json, value);
}

absl::StatusOr<ValueView> ParsedMapValueInterface::Get(
    ValueManager& value_manager, ValueView key, Value& scratch) const {
  ValueView value;
  bool ok;
  CEL_ASSIGN_OR_RETURN(std::tie(value, ok), Find(value_manager, key, scratch));
  if (ABSL_PREDICT_FALSE(!ok)) {
    switch (value.kind()) {
      case ValueKind::kError:
        ABSL_FALLTHROUGH_INTENDED;
      case ValueKind::kUnknown:
        return value;
      default:
        return NoSuchKeyError(key);
    }
  }
  return value;
}

absl::StatusOr<std::pair<ValueView, bool>> ParsedMapValueInterface::Find(
    ValueManager& value_manager, ValueView key, Value& scratch) const {
  switch (key.kind()) {
    case ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUnknown:
      scratch = Value(key);
      return std::pair{scratch, false};
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
  CEL_ASSIGN_OR_RETURN(auto value, FindImpl(value_manager, key, scratch));
  if (value.has_value()) {
    return std::pair{*value, true};
  }
  return std::pair{NullValueView{}, false};
}

absl::StatusOr<ValueView> ParsedMapValueInterface::Has(
    ValueManager& value_manager, ValueView key, Value& scratch) const {
  switch (key.kind()) {
    case ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUnknown:
      scratch = Value{key};
      return scratch;
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
  return BoolValueView(has);
}

absl::Status ParsedMapValueInterface::ForEach(ValueManager& value_manager,
                                              ForEachCallback callback) const {
  CEL_ASSIGN_OR_RETURN(auto iterator, NewIterator(value_manager));
  while (iterator->HasNext()) {
    Value key_scratch;
    Value value_scratch;
    CEL_ASSIGN_OR_RETURN(auto key, iterator->Next(key_scratch));
    CEL_ASSIGN_OR_RETURN(auto value, Get(value_manager, key, value_scratch));
    CEL_ASSIGN_OR_RETURN(auto ok, callback(key, value));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<ValueView> ParsedMapValueInterface::Equal(
    ValueManager& value_manager, ValueView other, Value& scratch) const {
  if (auto list_value = As<MapValueView>(other); list_value.has_value()) {
    return MapValueEqual(value_manager, *this, *list_value, scratch);
  }
  return BoolValueView{false};
}

}  // namespace cel
