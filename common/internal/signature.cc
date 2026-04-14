// Copyright 2026 Google LLC
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

#include "common/internal/signature.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "common/type.h"
#include "common/type_kind.h"
#include "internal/status_macros.h"

namespace cel::common_internal {

namespace {

void AppendEscaped(std::string* result, std::string_view str, bool escape_dot) {
  for (char c : str) {
    switch (c) {
      case '\\':
      case '(':
      case ')':
      case '<':
      case '>':
      case '"':
      case ',':
      case '~':
        result->push_back('\\');
        break;
      case '.':
        if (escape_dot) {
          result->push_back('\\');
        }
        break;
    }
    result->push_back(c);
  }
}

absl::Status AppendTypeParameters(std::string* result, const Type& type);

// Recursively appends a string representation of the given `type` to `result`.
// Type parameters are enclosed in angle brackets and separated by commas.

// Grammar:
//   TypeDesc            = NamespaceIdentifier [ "<" TypeList ">" ] ;
//   NamespaceIdentifier = [ "." ] Identifier { "." Identifier } ;
//   TypeList            = TypeElem { "," TypeElem } ;
//   TypeElem            = TypeDesc | TypeParam
//   TypeParam           = "~" Alpha ;
//   Identifier          = ( Alpha | "_" ) { AlphaNumeric | "_" } ;
//   (* Terminals *)
//   Alpha               = "a"..."z" | "A"..."Z" ;
//   Digit               = "0"..."9" ;
//   AlphaNumeric        = Alpha | Digit ;
//
// For compatibility, the implementation allows unexpected characters in
// type names and parameters and escapes them with a backslash.
absl::Status AppendTypeDesc(std::string* result, const Type& type) {
  switch (type.kind()) {
    case TypeKind::kNull:
      absl::StrAppend(result, "null");
      break;
    case TypeKind::kBool:
      absl::StrAppend(result, "bool");
      break;
    case TypeKind::kInt:
      absl::StrAppend(result, "int");
      break;
    case TypeKind::kUint:
      absl::StrAppend(result, "uint");
      break;
    case TypeKind::kDouble:
      absl::StrAppend(result, "double");
      break;
    case TypeKind::kString:
      absl::StrAppend(result, "string");
      break;
    case TypeKind::kBytes:
      absl::StrAppend(result, "bytes");
      break;
    case TypeKind::kDuration:
      absl::StrAppend(result, "duration");
      break;
    case TypeKind::kTimestamp:
      absl::StrAppend(result, "timestamp");
      break;
    case TypeKind::kAny:
      absl::StrAppend(result, "any");
      break;
    case TypeKind::kDyn:
      absl::StrAppend(result, "dyn");
      break;
    case TypeKind::kBoolWrapper:
      absl::StrAppend(result, "wrapper<bool>");
      break;
    case TypeKind::kIntWrapper:
      absl::StrAppend(result, "wrapper<int>");
      break;
    case TypeKind::kUintWrapper:
      absl::StrAppend(result, "wrapper<uint>");
      break;
    case TypeKind::kDoubleWrapper:
      absl::StrAppend(result, "wrapper<double>");
      break;
    case TypeKind::kStringWrapper:
      absl::StrAppend(result, "wrapper<string>");
      break;
    case TypeKind::kBytesWrapper:
      absl::StrAppend(result, "wrapper<bytes>");
      break;
    case TypeKind::kList:
      absl::StrAppend(result, "list");
      CEL_RETURN_IF_ERROR(AppendTypeParameters(result, type));
      break;
    case TypeKind::kMap:
      absl::StrAppend(result, "map");
      CEL_RETURN_IF_ERROR(AppendTypeParameters(result, type));
      break;
    case TypeKind::kFunction:
      absl::StrAppend(result, "function");
      CEL_RETURN_IF_ERROR(AppendTypeParameters(result, type));
      break;
    case TypeKind::kType:
      absl::StrAppend(result, "type");
      CEL_RETURN_IF_ERROR(AppendTypeParameters(result, type));
      break;
    case TypeKind::kTypeParam:
      absl::StrAppend(result, "~");
      AppendEscaped(result, type.GetTypeParam().name(), /*escape_dot=*/true);
      break;
    case TypeKind::kOpaque:
      AppendEscaped(result, type.name(), /*escape_dot=*/false);
      CEL_RETURN_IF_ERROR(AppendTypeParameters(result, type));
      break;
    case TypeKind::kStruct:
      AppendEscaped(result, type.name(), /*escape_dot=*/false);
      CEL_RETURN_IF_ERROR(AppendTypeParameters(result, type));
      break;
    default:
      return absl::InvalidArgumentError(
          absl::StrFormat("Type kind: %s is not supported in CEL declarations",
                          type.DebugString()));
  }
  return absl::OkStatus();
}

absl::Status AppendTypeParameters(std::string* result, const Type& type) {
  const auto& parameters = type.GetParameters();
  if (!parameters.empty()) {
    result->push_back('<');
    for (size_t i = 0; i < parameters.size(); ++i) {
      CEL_RETURN_IF_ERROR(AppendTypeDesc(result, parameters[i]));
      if (i < parameters.size() - 1) {
        result->push_back(',');
      }
    }
    result->push_back('>');
  }
  return absl::OkStatus();
}
}  // namespace

absl::StatusOr<std::string> MakeTypeSignature(const Type& type) {
  std::string result;
  CEL_RETURN_IF_ERROR(AppendTypeDesc(&result, type));
  return result;
}

absl::StatusOr<std::string> MakeOverloadSignature(
    std::string_view function_name, const std::vector<Type>& args,
    bool is_member) {
  std::string result;
  if (is_member) {
    if (!args.empty()) {
      CEL_RETURN_IF_ERROR(AppendTypeDesc(&result, args[0]));
    } else {
      return absl::InvalidArgumentError("Member function with no receiver");
    }
    result.push_back('.');
  }
  AppendEscaped(&result, function_name, /*escape_dot=*/true);
  result.push_back('(');
  for (size_t i = is_member ? 1 : 0; i < args.size(); ++i) {
    CEL_RETURN_IF_ERROR(AppendTypeDesc(&result, args[i]));
    if (i < args.size() - 1) {
      result.push_back(',');
    }
  }
  result.push_back(')');

  return result;
}
}  // namespace cel::common_internal
