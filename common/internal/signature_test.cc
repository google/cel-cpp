#include "common/internal/signature.h"
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

#include "absl/base/no_destructor.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/type.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "google/protobuf/arena.h"

namespace cel::common_internal {
namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::cel::internal::GetTestingDescriptorPool;
using ::testing::HasSubstr;
using ::testing::ValuesIn;

google::protobuf::Arena* GetTestArena() {
  static absl::NoDestructor<google::protobuf::Arena> arena;
  return &*arena;
}

struct TypeSignatureTestCase {
  Type type;
  std::string expected_signature;
  std::string expected_error;
};

using TypeSignatureTest = testing::TestWithParam<TypeSignatureTestCase>;

TEST_P(TypeSignatureTest, TypeSignature) {
  const auto& param = GetParam();

  absl::StatusOr<std::string> signature =
      common_internal::MakeTypeSignature(param.type);
  if (!param.expected_error.empty()) {
    EXPECT_THAT(signature, StatusIs(absl::StatusCode::kInvalidArgument,
                                    HasSubstr(param.expected_error)));
  } else {
    EXPECT_THAT(signature, IsOkAndHolds(param.expected_signature));
  }
}

std::vector<TypeSignatureTestCase> GetTypeSignatureTestCases() {
  return {
      {
          .type = StringType{},
          .expected_signature = "string",
      },
      {
          .type = IntType{},
          .expected_signature = "int",
      },
      {
          .type = ListType(GetTestArena(), StringType{}),
          .expected_signature = "list<string>",
      },
      {
          .type = ListType(GetTestArena(), TypeParamType("A")),
          .expected_signature = "list<~A>",
      },
      {
          .type = MapType(GetTestArena(), IntType{}, DynType{}),
          .expected_signature = "map<int,dyn>",
      },
      {
          .type =
              MapType(GetTestArena(), TypeParamType("B"), TypeParamType("C")),
          .expected_signature = "map<~B,~C>",
      },
      {
          .type = OpaqueType(
              GetTestArena(), "bar",
              {FunctionType(GetTestArena(), TypeParamType("D"), {})}),
          .expected_signature = "bar<function<~D>>",
      },
      {
          .type = AnyType{},
          .expected_signature = "any",
      },
      {
          .type = DurationType{},
          .expected_signature = "duration",
      },
      {
          .type = TimestampType{},
          .expected_signature = "timestamp",
      },
      {
          .type = IntWrapperType{},
          .expected_signature = "wrapper<int>",
      },
      {
          .type = MessageType(GetTestingDescriptorPool()->FindMessageTypeByName(
              "cel.expr.conformance.proto3.TestAllTypes")),
          .expected_signature = "cel.expr.conformance.proto3.TestAllTypes",
      },
      {
          .type = ListType(GetTestArena(), TypeParamType(R"(a,b.<C>.(d)\e)")),
          .expected_signature = R"(list<~a\,b\.\<C\>\.\(d\)\\e>)",
      },
      {
          .type = UnknownType{},
          .expected_error =
              "Type kind: *unknown* is not supported in CEL declarations",
      },
      {
          .type = ErrorType{},
          .expected_error =
              "Type kind: *error* is not supported in CEL declarations",
      },
  };
}

INSTANTIATE_TEST_SUITE_P(TypeIdTest, TypeSignatureTest,
                         ValuesIn(GetTypeSignatureTestCases()));

struct OverloadSignatureTestCase {
  std::string function_name = "hello";
  std::vector<Type> args;
  bool is_member = false;
  std::string expected_signature;
  std::string expected_error;
};

using OverloadSignatureTest = testing::TestWithParam<OverloadSignatureTestCase>;

TEST_P(OverloadSignatureTest, OverloadSignature) {
  const auto& param = GetParam();

  absl::StatusOr<std::string> signature =
      common_internal::MakeOverloadSignature(param.function_name, param.args,
                                             param.is_member);
  if (!param.expected_error.empty()) {
    EXPECT_THAT(signature, StatusIs(absl::StatusCode::kInvalidArgument,
                                    HasSubstr(param.expected_error)));
  } else {
    EXPECT_THAT(signature, IsOkAndHolds(param.expected_signature));
  }
}

std::vector<OverloadSignatureTestCase> GetOverloadSignatureTestCases() {
  return {
      {
          .args = {StringType{}},
          .expected_signature = "hello(string)",
      },
      {
          .args = {IntType{}, UintType{}},
          .expected_signature = "hello(int,uint)",
      },
      {
          .args = {ListType(GetTestArena(), StringType{})},
          .expected_signature = "hello(list<string>)",
      },
      {
          .args = {ListType(GetTestArena(), TypeParamType("A"))},
          .expected_signature = "hello(list<~A>)",
      },
      {
          .args = {MapType(GetTestArena(), IntType{}, DynType{})},
          .expected_signature = "hello(map<int,dyn>)",
      },
      {
          .args = {MapType(GetTestArena(), TypeParamType("B"),
                           TypeParamType("C"))},
          .expected_signature = "hello(map<~B,~C>)",
      },
      {
          .args = {OpaqueType(
              GetTestArena(), "bar",
              {FunctionType(GetTestArena(), TypeParamType("D"), {})})},
          .expected_signature = "hello(bar<function<~D>>)",
      },
      {
          .args = {AnyType{}},
          .expected_signature = "hello(any)",
      },
      {
          .args = {DurationType{}},
          .expected_signature = "hello(duration)",
      },
      {
          .args = {TimestampType{}},
          .expected_signature = "hello(timestamp)",
      },
      {
          .args = {IntWrapperType{}},
          .expected_signature = "hello(wrapper<int>)",
      },
      {
          .args = {MessageType(
              GetTestingDescriptorPool()->FindMessageTypeByName(
                  "cel.expr.conformance.proto3.TestAllTypes"))},
          .expected_signature =
              "hello(cel.expr.conformance.proto3.TestAllTypes)",
      },
      {.args = {},
       .is_member = true,
       .expected_error = "Member function with no receiver"},
      {
          .args = {StringType{}},
          .is_member = true,
          .expected_signature = "string.hello()",
      },
      {
          .args = {StringType{}, ListType(GetTestArena(), BoolType{})},
          .is_member = true,
          .expected_signature = "string.hello(list<bool>)",
      },
      {
          .args = {StringType{}, BoolType{}, DynType{}},
          .is_member = true,
          .expected_signature = "string.hello(bool,dyn)",
      },
      {
          .function_name = R"(h.(e),l<l>\o)",
          .args = {StringType{},
                   ListType(GetTestArena(), TypeParamType(R"(a,b.<C>.(d)\e)"))},
          .is_member = true,
          .expected_signature =
              R"(string.h\.\(e\)\,l\<l\>\\o(list<~a\,b\.\<C\>\.\(d\)\\e>))",
      },
  };
}

INSTANTIATE_TEST_SUITE_P(OverloadIdTest, OverloadSignatureTest,
                         ValuesIn(GetOverloadSignatureTestCases()));

}  // namespace
}  // namespace cel::common_internal
