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

#include "eval/public/string_extension_func_registrar.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "eval/public/cel_function_adapter.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {

using google::protobuf::Arena;

constexpr char kEmptySeparator[] = "";

CelValue SplitWithLimit(Arena* arena, const CelValue::StringHolder value,
                        const CelValue::StringHolder delimiter, int64_t limit) {
  // As per specifications[1]. return empty list in case limit is set to 0.
  // 1. https://pkg.go.dev/github.com/google/cel-go/ext#Strings
  std::vector<std::string> string_split = {};
  if (limit < 0) {
    // perform regular split operation in case of limit < 0
    string_split = absl::StrSplit(value.value(), delimiter.value());
  } else if (limit > 0) {
    // The absl::MaxSplits generate at max limit + 1 number of elements where as
    // it is suppose to return limit nunmber of elements as per
    // specifications[1].
    // To resolve the inconsistency passing limit-1 as input to absl::MaxSplits
    // 1. https://pkg.go.dev/github.com/google/cel-go/ext#Strings
    string_split = absl::StrSplit(
        value.value(), absl::MaxSplits(delimiter.value(), limit - 1));
  }
  std::vector<CelValue> cel_list;
  cel_list.reserve(string_split.size());
  for (const std::string& substring : string_split) {
    cel_list.push_back(
        CelValue::CreateString(Arena::Create<std::string>(arena, substring)));
  }
  auto result = CelValue::CreateList(
      Arena::Create<ContainerBackedListImpl>(arena, cel_list));
  return result;
}

CelValue Split(Arena* arena, CelValue::StringHolder value,
               CelValue::StringHolder delimiter) {
  return SplitWithLimit(arena, value, delimiter, -1);
}

CelValue::StringHolder JoinWithSeparator(Arena* arena, const CelValue& value,
                                         absl::string_view separator) {
  const CelList* cel_list = value.ListOrDie();
  std::vector<std::string_view> string_list;
  string_list.reserve(cel_list->size());
  for (int i = 0; i < cel_list->size(); i++) {
    string_list.push_back(cel_list->Get(arena, i).StringOrDie().value());
  }
  auto result =
      Arena::Create<std::string>(arena, absl::StrJoin(string_list, separator));
  return CelValue::StringHolder(result);
}

CelValue::StringHolder Join(Arena* arena, const CelValue& value) {
  return JoinWithSeparator(arena, value, kEmptySeparator);
}

CelValue::StringHolder LowerAscii(Arena* arena,
                                  const CelValue::StringHolder value) {
  auto result =
      Arena::Create<std::string>(arena, absl::AsciiStrToLower(value.value()));
  return CelValue::StringHolder(result);
}

absl::Status RegisterStringExtensionFunctions(
    CelFunctionRegistry* registry, const InterpreterOptions& options) {
  if (options.enable_string_concat) {
    CEL_RETURN_IF_ERROR(
        (FunctionAdapter<CelValue::StringHolder, CelValue>::CreateAndRegister(
            "join", true,
            [](Arena* arena, CelValue value) -> CelValue::StringHolder {
              return Join(arena, value);
            },
            registry)));
    CEL_RETURN_IF_ERROR((
        FunctionAdapter<CelValue::StringHolder, CelValue,
                        CelValue::StringHolder>::
            CreateAndRegister(
                "join", true,
                [](Arena* arena, CelValue value,
                   CelValue::StringHolder separator) -> CelValue::StringHolder {
                  return JoinWithSeparator(arena, value, separator.value());
                },
                registry)));
  }
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<CelValue, CelValue::StringHolder,
                       CelValue::StringHolder>::
           CreateAndRegister(
               "split", true,
               [](Arena* arena, CelValue::StringHolder str,
                  CelValue::StringHolder delimiter) -> CelValue {
                 return Split(arena, str, delimiter);
               },
               registry)));

  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<CelValue, CelValue::StringHolder, CelValue::StringHolder,
                       int64_t>::
           CreateAndRegister(
               "split", true,
               [](Arena* arena, CelValue::StringHolder str,
                  CelValue::StringHolder delimiter, int64_t limit) -> CelValue {
                 return SplitWithLimit(arena, str, delimiter, limit);
               },
               registry)));
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<CelValue::StringHolder, CelValue::StringHolder>::
           CreateAndRegister(
               "lowerAscii", true,
               [](Arena* arena, CelValue::StringHolder str)
                   -> CelValue::StringHolder { return LowerAscii(arena, str); },
               registry)));
  return absl::OkStatus();
}
}  // namespace google::api::expr::runtime
