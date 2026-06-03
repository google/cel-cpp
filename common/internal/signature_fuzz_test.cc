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

#include <string>
#include <vector>

#include "cel/expr/checked.pb.h"
#include "testing/fuzzing/fuzztest.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "common/ast.h"
#include "common/internal/signature.h"
#include "common/type.h"
#include "common/type_kind.h"
#include "common/type_proto.h"
#include "common/type_spec_resolver.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"

namespace cel::common_internal {
namespace {

std::vector<std::string> TypeSeeds() {
  return {
      "int",
      "list<string>",
      "map<int,dyn>",
      "google.protobuf.BoolValue",
      "~A",
      "list<~A>",
      "map<~B,~C>",
      "bar<function<~D>>",
      "any",
      "timestamp",
      "duration",
      "bool_wrapper",
      "int_wrapper",
      "uint_wrapper",
      "cel.expr.conformance.proto3.TestAllTypes",
      "list<~a\\,b\\.\\<C\\>\\.\\(d\\)\\\\e>",
  };
}

std::vector<std::string> FunctionSeeds() {
  return {
      "hello(string)",
      "hello(int)",
      "hello(uint)",
      "hello(double)",
      "hello(bytes)",
      "hello(any)",
      "hello(timestamp)",
      "hello(duration)",
      "hello(bool_wrapper)",
      "hello(int_wrapper)",
      "hello(uint_wrapper)",
      "hello(cel.expr.conformance.proto3.TestAllTypes)",
      "string.hello()",
      "string.hello(bool)",
      "string.hello(bool,dyn)",
      "bar<~dummy\\.type>.hello()",
      "inspect(type<string>)",
      "h.(e),l<l>\\o(string,bool)",
  };
}

bool HasWhitespace(std::string_view str) {
  for (char c : str) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      return true;
    }
  }
  return false;
}

bool IsValidIdentifier(std::string_view name) {
  if (name.empty()) return false;
  if (absl::StrContains(name, "..")) return false;
  if (name.front() == '.' || name.back() == '.') return false;
  for (char c : name) {
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '.')) {
      return false;
    }
  }
  return true;
}

bool IsValidTypeParamName(std::string_view name) {
  if (name.empty()) return false;
  for (char c : name) {
    if (c < 33 || c > 126) return false;
  }
  return true;
}

bool IsValidForSignature(const Type& type) {
  switch (type.kind()) {
    case TypeKind::kOpaque:
    case TypeKind::kStruct:
      if (!IsValidIdentifier(type.name())) return false;
      break;
    case TypeKind::kTypeParam:
      if (!IsValidTypeParamName(type.name())) return false;
      break;
    default:
      break;
  }
  for (const auto& param : type.GetParameters()) {
    if (!IsValidForSignature(param)) return false;
  }
  return true;
}

void FuzzMakeTypeSignature(const cel::expr::Type& type_pb) {
  google::protobuf::Arena arena;
  const google::protobuf::DescriptorPool* pool = internal::GetTestingDescriptorPool();
  if (pool == nullptr) return;
  auto type = TypeFromProto(type_pb, pool, &arena);
  if (type.ok() && IsValidForSignature(*type)) {
    auto reserialized = MakeTypeSignature(*type);
    if (reserialized.ok()) {
      auto result2 = ParseType(*reserialized, &arena, *pool);
      EXPECT_TRUE(result2.ok());
      if (result2.ok() && !HasWhitespace(*reserialized)) {
        EXPECT_EQ(*type, *result2);
      }
    }
  }
}
FUZZ_TEST(SignatureFuzz, FuzzMakeTypeSignature);

void FuzzMakeOverloadSignature(
    const std::string& function_name,
    const std::vector<cel::expr::Type>& args_pb, bool is_member) {
  google::protobuf::Arena arena;
  const google::protobuf::DescriptorPool* pool = internal::GetTestingDescriptorPool();
  if (pool == nullptr) return;

  if (!IsValidIdentifier(function_name)) return;

  std::vector<Type> args;
  args.reserve(args_pb.size());
  for (const auto& arg_pb : args_pb) {
    auto arg = TypeFromProto(arg_pb, pool, &arena);
    if (!arg.ok() || !IsValidForSignature(*arg)) return;
    args.push_back(*arg);
  }

  auto reserialized = MakeOverloadSignature(function_name, args, is_member);
  if (reserialized.ok()) {
    auto result = ParseFunctionSignature(*reserialized);
    EXPECT_TRUE(result.ok());
  }
}
FUZZ_TEST(SignatureFuzz, FuzzMakeOverloadSignature);

void FuzzParseType(const std::string& signature) {
  google::protobuf::Arena arena;
  const google::protobuf::DescriptorPool* pool = internal::GetTestingDescriptorPool();
  if (pool == nullptr) return;
  auto result = ParseType(signature, &arena, *pool);
  if (result.ok()) {
    auto reserialized = MakeTypeSignature(*result);
    if (reserialized.ok()) {
      auto result2 = ParseType(*reserialized, &arena, *pool);
      // We expect the reserialized signature to parse successfully.
      EXPECT_TRUE(result2.ok());
      if (result2.ok() && !HasWhitespace(signature)) {
        EXPECT_EQ(*result, *result2);
      }
    }
  }
}
FUZZ_TEST(SignatureFuzz, FuzzParseType)
    .WithDomains(fuzztest::Arbitrary<std::string>().WithSeeds(TypeSeeds()));

void FuzzParseFunctionSignature(const std::string& signature) {
  google::protobuf::Arena arena;
  const google::protobuf::DescriptorPool* pool = internal::GetTestingDescriptorPool();
  if (pool == nullptr) return;

  auto result = ParseFunctionSignature(signature);
  if (result.ok()) {
    // If the parse is successful, try to convert the parsed argument specs to
    // Type, serialize back to an overload signature, and re-parse.
    std::vector<Type> converted_args;
    bool all_converted = true;
    if (result->signature_type.has_function()) {
      const auto& func_spec = result->signature_type.function();
      for (const auto& arg_spec : func_spec.arg_types()) {
        auto converted = ConvertTypeSpecToType(arg_spec, &arena, *pool);
        if (!converted.ok()) {
          all_converted = false;
          break;
        }
        converted_args.push_back(*converted);
      }
    } else {
      all_converted = false;
    }

    if (all_converted) {
      auto reserialized = MakeOverloadSignature(
          result->function_name, converted_args, result->is_member);
      if (reserialized.ok()) {
        auto result2 = ParseFunctionSignature(*reserialized);
        EXPECT_TRUE(result2.ok());
      }
    }
  }
}
FUZZ_TEST(SignatureFuzz, FuzzParseFunctionSignature)
    .WithDomains(fuzztest::Arbitrary<std::string>().WithSeeds(FunctionSeeds()));

}  // namespace
}  // namespace cel::common_internal
