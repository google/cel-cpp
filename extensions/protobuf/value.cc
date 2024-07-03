// Copyright 2024 Google LLC
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

#include "extensions/protobuf/value.h"

#include <limits>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "common/casting.h"
#include "common/value.h"
#include "google/protobuf/descriptor.h"

namespace cel::extensions {

absl::StatusOr<int> ProtoEnumFromValue(
    const Value& value, absl::Nonnull<const google::protobuf::EnumDescriptor*> desc) {
  if (desc->full_name() == "google.protobuf.NullValue") {
    if (InstanceOf<NullValue>(value) || InstanceOf<IntValue>(value)) {
      return 0;
    }
    return TypeConversionError(value.GetTypeName(), desc->full_name())
        .NativeValue();
  }
  if (auto int_value = As<IntValue>(value); int_value) {
    if (int_value->NativeValue() >= 0 &&
        int_value->NativeValue() <= std::numeric_limits<int>::max()) {
      const auto* value_desc =
          desc->FindValueByNumber(static_cast<int>(int_value->NativeValue()));
      if (value_desc != nullptr) {
        return value_desc->number();
      }
    }
    return absl::NotFoundError(absl::StrCat("enum `", desc->full_name(),
                                            "` has no value with number ",
                                            int_value->NativeValue()));
  }
  return TypeConversionError(value.GetTypeName(), desc->full_name())
      .NativeValue();
}

}  // namespace cel::extensions
