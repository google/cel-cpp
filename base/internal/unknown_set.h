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

#ifndef THIRD_PARTY_CEL_CPP_BASE_INTERNAL_UNKNOWN_SET_H_
#define THIRD_PARTY_CEL_CPP_BASE_INTERNAL_UNKNOWN_SET_H_

#include <utility>

#include "absl/base/attributes.h"
#include "base/attribute_set.h"
#include "base/function_result_set.h"

namespace cel::base_internal {

// For compatibility with the old API and to avoid unnecessary copying when
// converting between the old and new representations, we store the historical
// members of google::api::expr::runtime::UnknownSet in this struct for use with
// std::shared_ptr.
struct UnknownSetImpl final {
  UnknownSetImpl() = default;

  UnknownSetImpl(AttributeSet attributes, FunctionResultSet function_results)
      : attributes(std::move(attributes)),
        function_results(std::move(function_results)) {}

  explicit UnknownSetImpl(AttributeSet attributes)
      : attributes(std::move(attributes)) {}

  explicit UnknownSetImpl(FunctionResultSet function_results)
      : function_results(std::move(function_results)) {}

  AttributeSet attributes;
  FunctionResultSet function_results;
};

ABSL_ATTRIBUTE_PURE_FUNCTION const AttributeSet& EmptyAttributeSet();

ABSL_ATTRIBUTE_PURE_FUNCTION const FunctionResultSet& EmptyFunctionResultSet();

}  // namespace cel::base_internal

#endif  // THIRD_PARTY_CEL_CPP_BASE_INTERNAL_UNKNOWN_SET_H_
