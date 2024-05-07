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
#include "common/sized_input_view.h"
#include "common/type.h"

namespace cel {

namespace {

struct TypeFormatter {
  void operator()(std::string* out, const Type& type) const {
    out->append(type.DebugString());
  }
};

std::string FunctionDebugString(const Type& result,
                                absl::Span<const Type> args) {
  return absl::StrCat("(", absl::StrJoin(args, ", ", TypeFormatter{}), ") -> ",
                      result.DebugString());
}

absl::FixedArray<Type, 1> SizedInputViewToFixedArray(
    const SizedInputView<TypeView>& args) {
  absl::FixedArray<Type, 1> fixed_args(args.size());
  size_t index = 0;
  for (const auto& arg : args) {
    fixed_args[index++] = Type(arg);
  }
  ABSL_DCHECK_EQ(index, args.size());
  return fixed_args;
}

}  // namespace

FunctionType::FunctionType(MemoryManagerRef memory_manager, TypeView result,
                           const SizedInputView<TypeView>& args)
    : data_(memory_manager.MakeShared<common_internal::FunctionTypeData>(
          Type(result), SizedInputViewToFixedArray(std::move(args)))) {}

std::string FunctionType::DebugString() const {
  return FunctionDebugString(result(), args());
}

std::string FunctionTypeView::DebugString() const {
  return FunctionDebugString(result(), args());
}

}  // namespace cel
