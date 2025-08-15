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
#ifndef THIRD_PARTY_CEL_CPP_BASE_AST_INTERNAL_EXPR_H_
#define THIRD_PARTY_CEL_CPP_BASE_AST_INTERNAL_EXPR_H_

#include "common/ast/metadata.h"

namespace cel::ast_internal {

// Temporary aliases to allow for moving the metadata types.
using TypeKind = cel::TypeSpecKind;
using Type = cel::TypeSpec;
using Extension = cel::ExtensionSpec;
using ListType = cel::ListTypeSpec;
using MapType = cel::MapTypeSpec;
using FunctionType = cel::FunctionTypeSpec;
using AbstractType = cel::AbstractType;
using PrimitiveType = cel::PrimitiveType;
using PrimitiveTypeWrapper = cel::PrimitiveTypeWrapper;
using WellKnownType = cel::WellKnownTypeSpec;
using MessageType = cel::MessageTypeSpec;
using ParamType = cel::ParamTypeSpec;
using SourceInfo = cel::SourceInfo;
using ErrorType = cel::ErrorTypeSpec;
using DynamicType = cel::DynTypeSpec;
using NullType = cel::NullTypeSpec;
using Reference = cel::Reference;
using UnspecifiedType = cel::UnsetTypeSpec;

}  // namespace cel::ast_internal

#endif  // THIRD_PARTY_CEL_CPP_BASE_EXPR_H_
