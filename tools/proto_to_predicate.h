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

#ifndef THIRD_PARTY_CEL_CPP_TOOLS_PROTO_TO_PREDICATE_H_
#define THIRD_PARTY_CEL_CPP_TOOLS_PROTO_TO_PREDICATE_H_

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/ast.h"
#include "google/protobuf/message.h"

namespace cel::tools {

// Translates a Protocol Buffer message into a CEL AST representing a predicate.
//
// NOTE: The protocol message schemas used for policy definition should use
// `proto2` or `editions` (and not `proto3` implicit presence) to ensure correct
// behavior, as this library relies on field presence (via reflection) to
// identify which fields are explicitly set by the policy.
absl::StatusOr<Ast> ProtoToPredicateAst(absl::string_view input_name,
                                        const ::google::protobuf::Message& message);

// Translates a list of Protocol Buffer messages into a CEL AST representing a
// conjoined or alternate predicate.
//
// NOTE: The protocol message schemas used for policy definition should use
// `proto2` or `editions` (and not `proto3` implicit presence) to ensure correct
// behavior, as this library relies on field presence (via reflection) to
// identify which fields are explicitly set by the policy.
absl::StatusOr<Ast> ProtoToPredicateAst(
    absl::string_view input_name,
    absl::Span<const ::google::protobuf::Message* const> messages);

}  // namespace cel::tools

#endif  // THIRD_PARTY_CEL_CPP_TOOLS_PROTO_TO_PREDICATE_H_
