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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_ANY_H_
#define THIRD_PARTY_CEL_CPP_COMMON_ANY_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"

namespace cel {

// `Any` is a native C++ representation of `google.protobuf.Any`, without the
// protocol buffer dependency.
class Any final {
 public:
  Any() = default;

  Any(std::string type_url, absl::Cord value)
      : type_url_(std::move(type_url)), value_(std::move(value)) {}

  void set_type_url(std::string type_url) { type_url_ = std::move(type_url); }

  absl::string_view type_url() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return type_url_;
  }

  std::string release_type_url() {
    std::string type_url;
    type_url.swap(type_url_);
    return type_url;
  }

  void set_value(absl::Cord value) { value_ = std::move(value); }

  const absl::Cord& value() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return value_;
  }

  absl::Cord release_value() {
    absl::Cord value;
    value.swap(value_);
    return value;
  }

  std::string DebugString() const;

 private:
  std::string type_url_;
  absl::Cord value_;
};

inline std::ostream& operator<<(std::ostream& out, const Any& any) {
  return out << any.DebugString();
}

inline bool operator==(const Any& lhs, const Any& rhs) {
  return lhs.type_url() == rhs.type_url() && lhs.value() == rhs.value();
}

inline bool operator!=(const Any& lhs, const Any& rhs) {
  return !operator==(lhs, rhs);
}

inline Any MakeAny(std::string type_url, absl::Cord value) {
  Any any;
  any.set_type_url(std::move(type_url));
  any.set_value(std::move(value));
  return any;
}

inline Any MakeAny(std::string type_url, const std::string& value) {
  return MakeAny(std::move(type_url), absl::Cord(value));
}

inline constexpr absl::string_view kTypeGoogleApisComPrefix =
    "type.googleapis.com/";

inline std::string MakeTypeUrlWithPrefix(absl::string_view prefix,
                                         absl::string_view type_name) {
  return absl::StrCat(absl::StripSuffix(prefix, "/"), "/", type_name);
}

inline std::string MakeTypeUrl(absl::string_view type_name) {
  return MakeTypeUrlWithPrefix(kTypeGoogleApisComPrefix, type_name);
}

bool ParseTypeUrl(absl::string_view type_url,
                  absl::Nullable<absl::string_view*> prefix,
                  absl::Nullable<absl::string_view*> type_name);
inline bool ParseTypeUrl(absl::string_view type_url,
                         absl::Nullable<absl::string_view*> type_name) {
  return ParseTypeUrl(type_url, nullptr, type_name);
}
inline bool ParseTypeUrl(absl::string_view type_url) {
  return ParseTypeUrl(type_url, nullptr);
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_ANY_H_
