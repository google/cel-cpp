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

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/type.h"
#include "common/type_kind.h"

namespace cel::common_internal {

namespace {

void AppendEscaped(std::string* result, absl::string_view str,
                   bool escape_dot) {
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
        result->push_back(c);
        break;
      case '.':
        if (escape_dot) {
          result->push_back('\\');
        }
        result->push_back(c);
        break;
      default:
        result->push_back(c);
        break;
    }
  }
}

void AppendTypeParameters(std::string* result, const Type& type);

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
void AppendTypeDesc(std::string* result, const Type& type) {
  switch (type.kind()) {
    case TypeKind::kNull:
      absl::StrAppend(result, "null");
      return;
    case TypeKind::kBool:
      absl::StrAppend(result, "bool");
      return;
    case TypeKind::kInt:
      absl::StrAppend(result, "int");
      return;
    case TypeKind::kUint:
      absl::StrAppend(result, "uint");
      return;
    case TypeKind::kDouble:
      absl::StrAppend(result, "double");
      return;
    case TypeKind::kString:
      absl::StrAppend(result, "string");
      return;
    case TypeKind::kBytes:
      absl::StrAppend(result, "bytes");
      return;
    case TypeKind::kDuration:
      absl::StrAppend(result, "duration");
      return;
    case TypeKind::kTimestamp:
      absl::StrAppend(result, "timestamp");
      return;
    case TypeKind::kUnknown:
      absl::StrAppend(result, "unknown");
      return;
    case TypeKind::kError:
      absl::StrAppend(result, "error");
      return;
    case TypeKind::kAny:
      absl::StrAppend(result, "any");
      return;
    case TypeKind::kDyn:
      absl::StrAppend(result, "dyn");
      return;
    case TypeKind::kBoolWrapper:
      absl::StrAppend(result, "bool_wrapper");
      return;
    case TypeKind::kIntWrapper:
      absl::StrAppend(result, "int_wrapper");
      return;
    case TypeKind::kUintWrapper:
      absl::StrAppend(result, "uint_wrapper");
      return;
    case TypeKind::kDoubleWrapper:
      absl::StrAppend(result, "double_wrapper");
      return;
    case TypeKind::kStringWrapper:
      absl::StrAppend(result, "string_wrapper");
      return;
    case TypeKind::kBytesWrapper:
      absl::StrAppend(result, "bytes_wrapper");
      return;
    case TypeKind::kList:
      absl::StrAppend(result, "list");
      AppendTypeParameters(result, type);
      return;
    case TypeKind::kMap:
      absl::StrAppend(result, "map");
      AppendTypeParameters(result, type);
      return;
    case TypeKind::kFunction:
      absl::StrAppend(result, "function");
      AppendTypeParameters(result, type);
      return;
    case TypeKind::kEnum:
      absl::StrAppend(result, "enum");
      AppendTypeParameters(result, type);
      return;
    case TypeKind::kType:
      absl::StrAppend(result, "type");
      AppendTypeParameters(result, type);
      return;
    case TypeKind::kTypeParam:
      absl::StrAppend(result, "~");
      AppendEscaped(result, type.GetTypeParam().name(), /*escape_dot=*/true);
      return;
    case TypeKind::kOpaque:
      AppendEscaped(result, type.name(), /*escape_dot=*/false);
      AppendTypeParameters(result, type);
      return;
    default:  // This includes TypeKind::kStruct aka TypeKind::kTypeMessage
      AppendEscaped(result, type.name(), /*escape_dot=*/false);
      return;
  }
}

void AppendTypeParameters(std::string* result, const Type& type) {
  const auto& parameters = type.GetParameters();
  if (!parameters.empty()) {
    result->push_back('<');
    for (size_t i = 0; i < parameters.size(); ++i) {
      AppendTypeDesc(result, parameters[i]);
      if (i < parameters.size() - 1) {
        result->push_back(',');
      }
    }
    result->push_back('>');
  }
}
}  // namespace

std::string MakeTypeSignature(const Type& type) {
  std::string result;
  AppendTypeDesc(&result, type);
  return result;
}

std::string MakeOverloadSignature(std::string_view function_name,
                                  const std::vector<Type>& args,
                                  bool is_member) {
  std::string result;
  if (is_member) {
    if (!args.empty()) {
      AppendTypeDesc(&result, args[0]);
    } else {
      // This should never happen: a member function with no receiver.
      absl::StrAppend(&result, "error");
    }
    result.push_back('.');
  }
  AppendEscaped(&result, function_name, /*escape_dot=*/true);
  result.push_back('(');
  for (size_t i = is_member ? 1 : 0; i < args.size(); ++i) {
    AppendTypeDesc(&result, args[i]);
    if (i < args.size() - 1) {
      result.push_back(',');
    }
  }
  result.push_back(')');

  return result;
}
}  // namespace cel::common_internal
