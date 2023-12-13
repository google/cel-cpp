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

#include <memory>
#include <tuple>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "internal/status_macros.h"

namespace cel {

namespace {

absl::Status InvalidMapKeyTypeError(ValueKind kind) {
  return absl::InvalidArgumentError(
      absl::StrCat("Invalid map key type: '", ValueKindToString(kind), "'"));
}

absl::Status NoSuchKeyError(ValueView key) {
  return absl::NotFoundError(
      absl::StrCat("Key not found in map : ", key.DebugString()));
}

}  // namespace

absl::Status MapValueInterface::CheckKey(ValueView key) {
  switch (key.kind()) {
    case ValueKind::kBool:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kInt:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUint:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kString:
      return absl::OkStatus();
    default:
      return InvalidMapKeyTypeError(key.kind());
  }
}

absl::StatusOr<ValueView> MapValueInterface::Get(ValueFactory& value_factory,
                                                 ValueView key,
                                                 Value& scratch) const {
  ValueView value;
  bool ok;
  CEL_ASSIGN_OR_RETURN(std::tie(value, ok), Find(value_factory, key, scratch));
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

absl::StatusOr<std::pair<ValueView, bool>> MapValueInterface::Find(
    ValueFactory& value_factory, ValueView key, Value& scratch) const {
  switch (key.kind()) {
    case ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUnknown:
      return std::pair{key, false};
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
  CEL_ASSIGN_OR_RETURN(auto value, FindImpl(value_factory, key, scratch));
  if (value.has_value()) {
    return std::pair{*value, true};
  }
  return std::pair{NullValueView{}, false};
}

absl::StatusOr<ValueView> MapValueInterface::Has(ValueView key) const {
  switch (key.kind()) {
    case ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUnknown:
      return key;
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
  CEL_ASSIGN_OR_RETURN(auto has, HasImpl(key));
  return BoolValueView(has);
}

absl::Status MapValueInterface::ForEach(ValueFactory& value_factory,
                                        ForEachCallback callback) const {
  CEL_ASSIGN_OR_RETURN(auto iterator, NewIterator(value_factory));
  while (iterator->HasNext()) {
    Value key_scratch;
    Value value_scratch;
    CEL_ASSIGN_OR_RETURN(auto key, iterator->Next(key_scratch));
    CEL_ASSIGN_OR_RETURN(auto value, Get(value_factory, key, value_scratch));
    CEL_ASSIGN_OR_RETURN(auto ok, callback(key, value));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

}  // namespace cel
