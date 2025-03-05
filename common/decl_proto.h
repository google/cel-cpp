// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_DECL_PROTO_H_
#define THIRD_PARTY_CEL_CPP_COMMON_DECL_PROTO_H_

#include "cel/expr/checked.pb.h"
#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "common/decl.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"

namespace cel {

// Creates a VariableDecl from a google.api.expr.Decl.IdentDecl proto.
absl::StatusOr<VariableDecl> VariableDeclFromProto(
    absl::string_view name, const cel::expr::Decl::IdentDecl& variable,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::Arena*> arena);

// Creates a FunctionDecl from a google.api.expr.Decl.FunctionDecl proto.
absl::StatusOr<FunctionDecl> FunctionDeclFromProto(
    absl::string_view name,
    const cel::expr::Decl::FunctionDecl& function,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::Arena*> arena);

// Creates a VariableDecl or FunctionDecl from a google.api.expr.Decl proto.
absl::StatusOr<absl::variant<VariableDecl, FunctionDecl>> DeclFromProto(
    const cel::expr::Decl& decl,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::Arena*> arena);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_DECL_PROTO_H_
