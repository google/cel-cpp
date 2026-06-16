// Copyright 2026 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_POLICY_CEL_POLICY_PARSER_H_
#define THIRD_PARTY_CEL_CPP_POLICY_CEL_POLICY_PARSER_H_

#include "absl/status/status.h"
#include "policy/cel_policy_parse_context.h"

namespace cel {

// A policy parser for a given policy format. The type `T` parameter is the
// representation of the input file format, such as `<YAML::Node>` for YAML.
//
// Parsers are intended to be stateless: all state, including the resulting
// policy and any issues encountered, should be kept in the context passed to
// the `ParsePolicy` method.
template <typename T>
class CelPolicyParser {
 public:
  virtual ~CelPolicyParser() = default;

  // Parses the input and populates a CelPolicy in the context.
  virtual absl::Status ParsePolicy(CelPolicyParseContext& ctx) const = 0;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_POLICY_CEL_POLICY_PARSER_H_
