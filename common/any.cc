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

#include "common/any.h"

#include <string>

#include "absl/base/nullability.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "internal/strings.h"

namespace cel {

std::string Any::DebugString() const {
  std::string value_scratch;
  absl::string_view value_view;
  if (auto flat = value().TryFlat(); flat.has_value()) {
    value_view = *flat;
  } else {
    value_scratch = static_cast<std::string>(value());
    value_view = value_scratch;
  }
  return absl::StrCat("google.protobuf.Any{type_url: ",
                      internal::FormatStringLiteral(type_url()),
                      ", value: ", internal::FormatBytesLiteral(value_view),
                      "}");
}

bool ParseTypeUrl(absl::string_view type_url,
                  absl::Nullable<absl::string_view*> prefix,
                  absl::Nullable<absl::string_view*> type_name) {
  auto pos = type_url.find_last_of('/');
  if (pos == absl::string_view::npos || pos + 1 == type_url.size()) {
    return false;
  }
  if (prefix) {
    *prefix = type_url.substr(0, pos + 1);
  }
  if (type_name) {
    *type_name = type_url.substr(pos + 1);
  }
  return true;
}

}  // namespace cel
