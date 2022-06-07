// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_TEST_TYPE_REGISTRY_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_TEST_TYPE_REGISTRY_H_

#include "eval/public/cel_type_registry.h"
namespace google::api::expr::runtime {

// Returns a static singleton type registry suitable for use in most
// tests directly creating CelExpressionFlatImpl instances.
const CelTypeRegistry& TestTypeRegistry();

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_TEST_TYPE_REGISTRY_H_
