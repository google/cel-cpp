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

#include "common/json.h"

#include <string>

#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "internal/copy_on_write.h"
#include "internal/no_destructor.h"

namespace cel {

internal::CopyOnWrite<typename JsonArray::Container> JsonArray::Empty() {
  static const internal::NoDestructor<internal::CopyOnWrite<Container>> empty;
  return empty.get();
}

internal::CopyOnWrite<typename JsonObject::Container> JsonObject::Empty() {
  static const internal::NoDestructor<internal::CopyOnWrite<Container>> empty;
  return empty.get();
}

Json JsonInt(int64_t value) {
  if (value < kJsonMinInt || value > kJsonMaxInt) {
    return JsonString(absl::StrCat(value));
  }
  return Json(static_cast<double>(value));
}

Json JsonUint(uint64_t value) {
  if (value > kJsonMaxUint) {
    return JsonString(absl::StrCat(value));
  }
  return Json(static_cast<double>(value));
}

Json JsonBytes(absl::string_view value) {
  return JsonString(absl::Base64Escape(value));
}

Json JsonBytes(const absl::Cord& value) {
  if (auto flat = value.TryFlat(); flat.has_value()) {
    return JsonBytes(*flat);
  }
  return JsonBytes(absl::string_view(static_cast<std::string>(value)));
}

}  // namespace cel
