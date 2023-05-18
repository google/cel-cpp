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

#include "extensions/protobuf/enum_value.h"

#include <limits>

#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/types/optional.h"
#include "extensions/protobuf/enum_type.h"

namespace cel::extensions {

const google::protobuf::EnumValueDescriptor* ProtoEnumValue::descriptor(
    const EnumValue& value) {
  ABSL_ASSERT(Is(value));
  auto number = value.number();
  if (ABSL_PREDICT_FALSE(number < std::numeric_limits<int>::min() ||
                         number > std::numeric_limits<int>::max())) {
    return nullptr;
  }
  return value.type().As<ProtoEnumType>()->descriptor().FindValueByNumber(
      static_cast<int>(number));
}

absl::optional<int> ProtoEnumValue::value_impl(
    const ProtoEnumType& type, int64_t number,
    const google::protobuf::EnumDescriptor* desc) {
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::nullopt;
  }
  if (ABSL_PREDICT_FALSE(number < std::numeric_limits<int>::min() ||
                         number > std::numeric_limits<int>::max())) {
    return absl::nullopt;
  }
  if (&type.descriptor() != desc) {
    return absl::nullopt;
  }
  if (desc->FindValueByNumber(static_cast<int>(number)) == nullptr) {
    return absl::nullopt;
  }
  return static_cast<int>(number);
}

}  // namespace cel::extensions
