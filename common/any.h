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

#include <string>
#include <utility>

#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"

namespace cel {

// `Any` is a native C++ representation of `google.protobuf.Any`, without the
// protocol buffer dependency.
class Any final {
 public:
  void set_type_url(std::string type_url) { type_url_ = std::move(type_url); }

  absl::string_view type_url() const { return type_url_; }

  void set_value(absl::Cord value) { value_ = std::move(value); }

  absl::Cord value() const { return value_; }

 private:
  std::string type_url_;
  absl::Cord value_;
};

inline Any MakeAny(std::string type_url, absl::Cord value) {
  Any any;
  any.set_type_url(std::move(type_url));
  any.set_value(std::move(value));
  return any;
}

inline Any MakeAny(std::string type_url, const std::string& value) {
  return MakeAny(std::move(type_url), absl::Cord(value));
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_ANY_H_
