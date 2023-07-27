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

#include "base/values/type_value.h"

#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "base/internal/data.h"

namespace cel {

namespace {

using base_internal::InlinedTypeValueVariant;
using base_internal::Metadata;

}  // namespace

CEL_INTERNAL_VALUE_IMPL(TypeValue);

std::string TypeValue::DebugString() const { return std::string(name()); }

bool TypeValue::Equals(const TypeValue& other) const {
  return name() == static_cast<const TypeValue&>(other).name();
}

absl::string_view TypeValue::name() const {
  switch (Metadata::GetInlineVariant<InlinedTypeValueVariant>(*this)) {
    case InlinedTypeValueVariant::kLegacy:
      return static_cast<const base_internal::LegacyTypeValue&>(*this).name();
    case InlinedTypeValueVariant::kModern:
      return static_cast<const base_internal::ModernTypeValue&>(*this).name();
  }
}

absl::StatusOr<Any> TypeValue::ConvertToAny(ValueFactory&) const {
  return absl::FailedPreconditionError(absl::StrCat(
      type()->name(), " cannot be serialized as google.protobuf.Any"));
}

absl::StatusOr<Json> TypeValue::ConvertToJson(ValueFactory&) const {
  return absl::FailedPreconditionError(absl::StrCat(
      type()->name(), " cannot be serialized as google.protobuf.Value"));
}

}  // namespace cel
