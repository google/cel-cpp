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

#include "common/values/message_value.h"

#include <utility>

#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/functional/overload.h"
#include "absl/log/absl_check.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "common/optional_ref.h"
#include "common/values/parsed_message_value.h"
#include "google/protobuf/descriptor.h"

namespace cel {

absl::Nonnull<const google::protobuf::Descriptor*> MessageValue::GetDescriptor() const {
  ABSL_CHECK(*this);  // Crash OK
  return absl::visit(
      absl::Overload(
          [](absl::monostate) -> absl::Nonnull<const google::protobuf::Descriptor*> {
            ABSL_UNREACHABLE();
          },
          [](const ParsedMessageValue& alternative)
              -> absl::Nonnull<const google::protobuf::Descriptor*> {
            return alternative.GetDescriptor();
          }),
      variant_);
}

cel::optional_ref<const ParsedMessageValue> MessageValue::AsParsed() const& {
  if (const auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

cel::optional_ref<const ParsedMessageValue> MessageValue::AsParsed() & {
  if (const auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedMessageValue> MessageValue::AsParsed() const&& {
  if (auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

absl::optional<ParsedMessageValue> MessageValue::AsParsed() && {
  if (auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

MessageValue::operator const ParsedMessageValue&() const& {
  ABSL_DCHECK(IsParsed());
  return absl::get<ParsedMessageValue>(variant_);
}

MessageValue::operator const ParsedMessageValue&() & {
  ABSL_DCHECK(IsParsed());
  return absl::get<ParsedMessageValue>(variant_);
}

MessageValue::operator ParsedMessageValue() const&& {
  ABSL_DCHECK(IsParsed());
  return absl::get<ParsedMessageValue>(std::move(variant_));
}

MessageValue::operator ParsedMessageValue() && {
  ABSL_DCHECK(IsParsed());
  return absl::get<ParsedMessageValue>(std::move(variant_));
}

}  // namespace cel
