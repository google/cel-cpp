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
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/types/optional.h"
#include "common/ast.h"
#include "common/type.h"
#include "common/type_kind.h"
#include "common/type_spec_resolver.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"

namespace cel::common_internal {

// Signature generator helper functions.
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
//
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
      absl::StrAppend(result, "bool_wrapper");
      break;
    case TypeKind::kIntWrapper:
      absl::StrAppend(result, "int_wrapper");
      break;
    case TypeKind::kUintWrapper:
      absl::StrAppend(result, "uint_wrapper");
      break;
    case TypeKind::kDoubleWrapper:
      absl::StrAppend(result, "double_wrapper");
      break;
    case TypeKind::kStringWrapper:
      absl::StrAppend(result, "string_wrapper");
      break;
    case TypeKind::kBytesWrapper:
      absl::StrAppend(result, "bytes_wrapper");
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

// Signature parser helper functions.
namespace {

std::string StripUnescapedWhitespace(std::string_view str) {
  std::string result;
  result.reserve(str.size());
  bool escaped = false;
  for (char c : str) {
    if (escaped) {
      result.push_back(c);
      escaped = false;
      continue;
    }
    if (c == '\\') {
      result.push_back(c);
      escaped = true;
      continue;
    }
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      continue;
    }
    result.push_back(c);
  }
  return result;
}

absl::optional<TypeSpec> ParseBuiltinOrWrapper(std::string_view name_str) {
  if (name_str == "null") return TypeSpec(NullTypeSpec());
  if (name_str == "bool") return TypeSpec(PrimitiveType::kBool);
  if (name_str == "int") return TypeSpec(PrimitiveType::kInt64);
  if (name_str == "uint") return TypeSpec(PrimitiveType::kUint64);
  if (name_str == "double") return TypeSpec(PrimitiveType::kDouble);
  if (name_str == "string") return TypeSpec(PrimitiveType::kString);
  if (name_str == "bytes") return TypeSpec(PrimitiveType::kBytes);
  if (name_str == "any" || name_str == "google.protobuf.Any")
    return TypeSpec(WellKnownTypeSpec::kAny);
  if (name_str == "timestamp" || name_str == "google.protobuf.Timestamp")
    return TypeSpec(WellKnownTypeSpec::kTimestamp);
  if (name_str == "duration" || name_str == "google.protobuf.Duration")
    return TypeSpec(WellKnownTypeSpec::kDuration);
  if (name_str == "dyn" || name_str == "google.protobuf.Value")
    return TypeSpec(DynTypeSpec());

  // Handle standard Protobuf well-known wrapper types to preserve
  // backward compatibility for users migrating YAML configuration files.
  if (name_str == "bool_wrapper" || name_str == "google.protobuf.BoolValue")
    return TypeSpec(PrimitiveTypeWrapper(PrimitiveType::kBool));
  if (name_str == "int_wrapper" || name_str == "google.protobuf.Int64Value" ||
      name_str == "google.protobuf.Int32Value")
    return TypeSpec(PrimitiveTypeWrapper(PrimitiveType::kInt64));
  if (name_str == "uint_wrapper" || name_str == "google.protobuf.UInt64Value" ||
      name_str == "google.protobuf.UInt32Value")
    return TypeSpec(PrimitiveTypeWrapper(PrimitiveType::kUint64));
  if (name_str == "double_wrapper" ||
      name_str == "google.protobuf.DoubleValue" ||
      name_str == "google.protobuf.FloatValue")
    return TypeSpec(PrimitiveTypeWrapper(PrimitiveType::kDouble));
  if (name_str == "string_wrapper" || name_str == "google.protobuf.StringValue")
    return TypeSpec(PrimitiveTypeWrapper(PrimitiveType::kString));
  if (name_str == "bytes_wrapper" || name_str == "google.protobuf.BytesValue")
    return TypeSpec(PrimitiveTypeWrapper(PrimitiveType::kBytes));

  if (name_str == "google.protobuf.ListValue") {
    return TypeSpec(ListTypeSpec(std::make_unique<TypeSpec>(DynTypeSpec())));
  }
  if (name_str == "google.protobuf.Struct") {
    return TypeSpec(
        MapTypeSpec(std::make_unique<TypeSpec>(PrimitiveType::kString),
                    std::make_unique<TypeSpec>(DynTypeSpec())));
  }

  return absl::nullopt;
}

std::string Unescape(std::string_view str) {
  size_t first_escape = str.find('\\');
  if (first_escape == std::string_view::npos) {
    return std::string(str);
  }
  std::string result;
  result.reserve(str.size());
  result.append(str.substr(0, first_escape));
  bool escaped = false;
  for (size_t i = first_escape; i < str.size(); ++i) {
    char c = str[i];
    if (escaped) {
      result.push_back(c);
      escaped = false;
    } else if (c == '\\') {
      escaped = true;
    } else {
      result.push_back(c);
    }
  }
  if (escaped) {
    result.push_back('\\');
  }
  return result;
}

class SignatureScanner {
 public:
  explicit SignatureScanner(std::string_view input,
                            std::string_view error_prefix = "Invalid signature")
      : input_(input), error_prefix_(error_prefix) {}

  absl::StatusOr<size_t> FindTopLevelChar(char target, bool find_last = false) {
    size_t found_idx = std::string_view::npos;
    int nesting = 0;
    bool escaped = false;
    // Scanning str for delimiter boundaries while ensuring
    // brackets are balanced and escape backslashes are bypassed.
    for (size_t i = 0; i < input_.size(); ++i) {
      char c = input_[i];
      if (escaped) {
        escaped = false;
        continue;
      }
      if (c == '\\') {
        escaped = true;
        continue;
      }
      if (c == target && nesting == 0) {
        if (find_last || found_idx == std::string_view::npos) {
          found_idx = i;
        }
      }
      if (c == '<') {
        nesting++;
      } else if (c == '>') {
        nesting--;
        if (nesting < 0) {
          return absl::InvalidArgumentError(
              absl::StrCat(error_prefix_, ": mismatched brackets"));
        }
      }
    }
    if (nesting != 0) {
      return absl::InvalidArgumentError(
          absl::StrCat(error_prefix_, ": mismatched brackets"));
    }
    return found_idx;
  }

  absl::StatusOr<std::vector<std::string_view>> SplitTopLevel(char delimiter) {
    std::vector<std::string_view> result;
    int nesting = 0;
    bool escaped = false;
    size_t start = 0;
    // Scanning str for delimiter while ensuring brackets are balanced and
    // escape backslashes are bypassed.
    for (size_t i = 0; i < input_.size(); ++i) {
      char c = input_[i];
      if (escaped) {
        escaped = false;
        continue;
      }
      if (c == '\\') {
        escaped = true;
        continue;
      }
      if (c == delimiter && nesting == 0) {
        result.push_back(input_.substr(start, i - start));
        start = i + 1;
      }
      if (c == '<') {
        nesting++;
      } else if (c == '>') {
        nesting--;
        if (nesting < 0) {
          return absl::InvalidArgumentError(
              absl::StrCat(error_prefix_, ": mismatched brackets"));
        }
      }
    }
    if (nesting != 0) {
      return absl::InvalidArgumentError(
          absl::StrCat(error_prefix_, ": mismatched brackets"));
    }
    result.push_back(input_.substr(start));
    return result;
  }

 private:
  std::string_view input_;
  std::string_view error_prefix_;
};

absl::StatusOr<std::vector<std::string_view>> SplitTypeList(
    std::string_view params) {
  return SignatureScanner(params, "Invalid type signature").SplitTopLevel(',');
}

absl::StatusOr<TypeSpec> ParseTypeSignature(std::string_view signature) {
  if (signature.empty()) {
    return absl::InvalidArgumentError("Empty type signature");
  }

  if (signature[0] == '~') {
    std::string_view param_name = signature.substr(1);
    if (param_name.empty()) {
      return absl::InvalidArgumentError(
          "Invalid type signature: invalid type parameter name");
    }
    CEL_ASSIGN_OR_RETURN(size_t less_idx,
                         SignatureScanner(param_name)
                             .FindTopLevelChar('<', /*find_last=*/false));
    CEL_ASSIGN_OR_RETURN(size_t comma_idx,
                         SignatureScanner(param_name)
                             .FindTopLevelChar(',', /*find_last=*/false));
    if (less_idx != std::string_view::npos ||
        comma_idx != std::string_view::npos) {
      return absl::InvalidArgumentError(
          "Invalid type signature: invalid type parameter name");
    }
    return TypeSpec(ParamTypeSpec(Unescape(param_name)));
  }

  CEL_ASSIGN_OR_RETURN(size_t less_idx,
                       SignatureScanner(signature, "Invalid type signature")
                           .FindTopLevelChar('<', /*find_last=*/false));

  std::string name_str;
  std::vector<TypeSpec> params;

  if (less_idx != std::string_view::npos) {
    // If the signature contains a '<', it must also contain a matching '>'.
    if (signature.back() != '>') {
      return absl::InvalidArgumentError(
          "Invalid type signature: missing closing >");
    }
    name_str = Unescape(signature.substr(0, less_idx));
    std::string_view params_str =
        signature.substr(less_idx + 1, signature.size() - less_idx - 2);
    CEL_ASSIGN_OR_RETURN(auto param_list, SplitTypeList(params_str));
    for (std::string_view param_str : param_list) {
      CEL_ASSIGN_OR_RETURN(auto param, ParseTypeSignature(param_str));
      params.push_back(std::move(param));
    }
  } else {
    name_str = Unescape(signature);
  }

  auto read_param_or_dyn = [&params](size_t index) {
    auto spec = std::make_unique<TypeSpec>(DynTypeSpec());
    if (params.size() > index) {
      *spec = std::move(params[index]);
    }
    return spec;
  };

  if (!params.empty()) {
    if (ParseBuiltinOrWrapper(name_str).has_value()) {
      return absl::InvalidArgumentError(
          absl::StrCat("Invalid type signature: ", name_str,
                       " cannot have type parameters"));
    }
  } else {
    if (auto builtin = ParseBuiltinOrWrapper(name_str); builtin.has_value()) {
      return *builtin;
    }
  }

  if (name_str == "type") {
    if (params.size() > 1) {
      return absl::InvalidArgumentError(
          "Invalid type signature: type expects at most 1 parameter");
    }
    return TypeSpec(read_param_or_dyn(0));
  }

  if (name_str == "list") {
    if (params.size() > 1) {
      return absl::InvalidArgumentError(
          "Invalid type signature: list expects at most 1 parameter");
    }
    return TypeSpec(ListTypeSpec(read_param_or_dyn(0)));
  }

  if (name_str == "map") {
    if (!params.empty() && params.size() != 2) {
      return absl::InvalidArgumentError(
          "Invalid type signature: map expects 0 or 2 parameters");
    }
    auto key = read_param_or_dyn(0);
    auto value = read_param_or_dyn(1);
    return TypeSpec(MapTypeSpec(std::move(key), std::move(value)));
  }

  if (name_str == "function") {
    auto result_type = read_param_or_dyn(0);
    std::vector<TypeSpec> arg_types;
    for (size_t i = 1; i < params.size(); ++i) {
      arg_types.push_back(std::move(params[i]));
    }
    return TypeSpec(
        FunctionTypeSpec(std::move(result_type), std::move(arg_types)));
  }

  if (name_str.empty() || absl::StrContains(name_str, "..")) {
    return absl::InvalidArgumentError(
        "Invalid type signature: invalid identifier");
  }

  return TypeSpec(AbstractType(name_str, std::move(params)));
}

}  // namespace

absl::StatusOr<ParsedFunctionOverload> ParseFunctionSignature(
    std::string_view signature) {
  std::string stripped_sig = StripUnescapedWhitespace(signature);
  if (stripped_sig.empty()) {
    return absl::InvalidArgumentError("Empty function signature");
  }

  CEL_ASSIGN_OR_RETURN(
      size_t paren_idx,
      SignatureScanner(stripped_sig, "Invalid function signature")
          .FindTopLevelChar('(', /*find_last=*/false));

  if (paren_idx == std::string_view::npos || stripped_sig.back() != ')') {
    return absl::InvalidArgumentError("Invalid function signature");
  }

  std::string_view prefix = std::string_view(stripped_sig).substr(0, paren_idx);
  std::string_view args_str =
      std::string_view(stripped_sig)
          .substr(paren_idx + 1, stripped_sig.size() - paren_idx - 2);

  std::vector<TypeSpec> arg_types;
  ParsedFunctionOverload out;

  CEL_ASSIGN_OR_RETURN(size_t dot_idx,
                       SignatureScanner(prefix, "Invalid function signature")
                           .FindTopLevelChar('.', /*find_last=*/true));

  if (dot_idx != std::string_view::npos) {
    out.is_member = true;
    std::string_view receiver_str = prefix.substr(0, dot_idx);
    std::string_view func_str = prefix.substr(dot_idx + 1);

    CEL_ASSIGN_OR_RETURN(auto receiver_param, ParseTypeSignature(receiver_str));
    arg_types.push_back(std::move(receiver_param));
    out.function_name = Unescape(func_str);
  } else {
    out.is_member = false;
    out.function_name = Unescape(prefix);
  }

  if (out.function_name.empty()) {
    return absl::InvalidArgumentError(
        "Invalid function signature: empty function name");
  }

  if (!args_str.empty()) {
    CEL_ASSIGN_OR_RETURN(auto arg_list, SplitTypeList(args_str));
    for (std::string_view arg_str : arg_list) {
      CEL_ASSIGN_OR_RETURN(auto arg_param, ParseTypeSignature(arg_str));
      arg_types.push_back(std::move(arg_param));
    }
  }

  auto result_type = std::make_unique<TypeSpec>(DynTypeSpec());
  out.signature_type =
      TypeSpec(FunctionTypeSpec(std::move(result_type), std::move(arg_types)));

  return out;
}

absl::StatusOr<Type> ParseType(std::string_view signature, google::protobuf::Arena* arena,
                               const google::protobuf::DescriptorPool& pool) {
  std::string stripped_sig = StripUnescapedWhitespace(signature);
  CEL_ASSIGN_OR_RETURN(auto type_spec, ParseTypeSignature(stripped_sig));
  return cel::ConvertTypeSpecToType(type_spec, arena, pool);
}

}  // namespace cel::common_internal
