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

#include "base/types/dyn_type.h"

#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace cel {

CEL_INTERNAL_TYPE_IMPL(DynType);

absl::Span<const absl::string_view> DynType::aliases() const {
  // Currently google.protobuf.Value also resolves to dyn.
  static constexpr absl::string_view kAliases[] = {"google.protobuf.Value"};
  return absl::MakeConstSpan(kAliases);
}

}  // namespace cel
