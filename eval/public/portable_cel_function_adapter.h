//  Copyright 2022 Google LLC
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//       https://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_PORTABLE_CEL_FUNCTION_ADAPTER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_PORTABLE_CEL_FUNCTION_ADAPTER_H_

#include "eval/public/cel_function_adapter_impl.h"

namespace google::api::expr::runtime {

// Portable version of the FunctionAdapter template utility.
//
// The PortableFunctionAdapter variation provides the same interface,
// but doesn't support unwrapping google::protobuf::Message values. See documentation on
// Function adapter for example usage.
//
// Most users should prefer using the standard FunctionAdapter.
template <typename ReturnType, typename... Arguments>
using PortableFunctionAdapter =
    internal::FunctionAdapter<internal::TypeCodeMatcher,
                              internal::ValueConverter, ReturnType,
                              Arguments...>;

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_PORTABLE_CEL_FUNCTION_ADAPTER_H_
