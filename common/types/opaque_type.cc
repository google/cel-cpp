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

#include <cstddef>
#include <string>
#include <utility>

#include "absl/container/fixed_array.h"
#include "absl/log/absl_check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type.h"

namespace cel {

namespace {

std::string OpaqueDebugString(absl::string_view name,
                              absl::Span<const Type> parameters) {
  if (parameters.empty()) {
    return std::string(name);
  }
  return absl::StrCat(
      name, "<", absl::StrJoin(parameters, ", ", absl::StreamFormatter()), ">");
}

absl::FixedArray<Type, 1> SizedInputViewToFixedArray(
    absl::Span<const Type> parameters) {
  absl::FixedArray<Type, 1> fixed_parameters(parameters.size());
  size_t index = 0;
  for (const auto& parameter : parameters) {
    fixed_parameters[index++] = Type(parameter);
  }
  ABSL_DCHECK_EQ(index, parameters.size());
  return fixed_parameters;
}

}  // namespace

OpaqueType::OpaqueType(MemoryManagerRef memory_manager, absl::string_view name,
                       absl::Span<const Type> parameters)
    : data_(memory_manager.MakeShared<common_internal::OpaqueTypeData>(
          std::string(name),
          SizedInputViewToFixedArray(std::move(parameters)))) {}

std::string OpaqueType::DebugString() const {
  return OpaqueDebugString(name(), parameters());
}

}  // namespace cel
