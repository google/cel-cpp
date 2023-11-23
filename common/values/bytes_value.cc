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

#include <string>

#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/value.h"
#include "internal/overloaded.h"
#include "internal/strings.h"

namespace cel {

namespace {

template <typename Bytes>
std::string BytesDebugString(const Bytes& value) {
  return value.NativeValue(internal::Overloaded{
      [](absl::string_view string) -> std::string {
        return internal::FormatBytesLiteral(string);
      },
      [](const absl::Cord& cord) -> std::string {
        if (auto flat = cord.TryFlat(); flat.has_value()) {
          return internal::FormatBytesLiteral(*flat);
        }
        return internal::FormatBytesLiteral(static_cast<std::string>(cord));
      }});
}

}  // namespace

std::string BytesValue::DebugString() const { return BytesDebugString(*this); }

std::string BytesValueView::DebugString() const {
  return BytesDebugString(*this);
}

}  // namespace cel
