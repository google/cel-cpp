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
//
// Type definitions for internal AST representation.
// CEL users should not directly depend on the definitions here.
// TODO(uncreated-issue/31): move to base/internal
#ifndef THIRD_PARTY_CEL_CPP_BASE_AST_INTERNAL_H_
#define THIRD_PARTY_CEL_CPP_BASE_AST_INTERNAL_H_

#include "base/ast_internal/expr.h"

namespace cel::ast::internal {

// Alias new moved types until clients are updated to use new namespace.
using NullValue = ast_internal::NullValue;
using Bytes = ast_internal::Bytes;
using ConstantKind = ast_internal::ConstantKind;
using Constant = ast_internal::Constant;
using Ident = ast_internal::Ident;
using Select = ast_internal::Select;
using Call = ast_internal::Call;
using CreateList = ast_internal::CreateList;
using CreateStruct = ast_internal::CreateStruct;
using Comprehension = ast_internal::Comprehension;
using ExprKind = ast_internal::ExprKind;
using Expr = ast_internal::Expr;
using SourceInfo = ast_internal::SourceInfo;
using ParsedExpr = ast_internal::ParsedExpr;
using PrimitiveType = ast_internal::PrimitiveType;
using WellKnownType = ast_internal::WellKnownType;
using ListType = ast_internal::ListType;
using MapType = ast_internal::MapType;
using FunctionType = ast_internal::FunctionType;
using AbstractType = ast_internal::AbstractType;
using PrimitiveTypeWrapper = ast_internal::PrimitiveTypeWrapper;
using MessageType = ast_internal::MessageType;
using ParamType = ast_internal::ParamType;
using ErrorType = ast_internal::ErrorType;
using DynamicType = ast_internal::DynamicType;
using TypeKind = ast_internal::TypeKind;
using Type = ast_internal::Type;
using Reference = ast_internal::Reference;
using CheckedExpr = ast_internal::CheckedExpr;

}  // namespace cel::ast::internal

#endif  // THIRD_PARTY_CEL_CPP_BASE_AST_INTERNAL_H_
