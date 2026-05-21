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

#include <cstddef>
#include <string>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "common/ast.h"
#include "common/type.h"
#include "common/type_kind.h"
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

void VerifyParsedMatchesType(const TypeSpec& parsed, const Type& original) {
  switch (original.kind()) {
    case TypeKind::kDyn:
      EXPECT_TRUE(parsed.has_dyn());
      break;
    case TypeKind::kNull:
      EXPECT_TRUE(parsed.has_null());
      break;
    case TypeKind::kBool:
      EXPECT_EQ(parsed.primitive(), PrimitiveType::kBool);
      break;
    case TypeKind::kInt:
      EXPECT_EQ(parsed.primitive(), PrimitiveType::kInt64);
      break;
    case TypeKind::kUint:
      EXPECT_EQ(parsed.primitive(), PrimitiveType::kUint64);
      break;
    case TypeKind::kDouble:
      EXPECT_EQ(parsed.primitive(), PrimitiveType::kDouble);
      break;
    case TypeKind::kString:
      EXPECT_EQ(parsed.primitive(), PrimitiveType::kString);
      break;
    case TypeKind::kBytes:
      EXPECT_EQ(parsed.primitive(), PrimitiveType::kBytes);
      break;
    case TypeKind::kAny:
      EXPECT_EQ(parsed.well_known(), WellKnownTypeSpec::kAny);
      break;
    case TypeKind::kTimestamp:
      EXPECT_EQ(parsed.well_known(), WellKnownTypeSpec::kTimestamp);
      break;
    case TypeKind::kDuration:
      EXPECT_EQ(parsed.well_known(), WellKnownTypeSpec::kDuration);
      break;
    case TypeKind::kList:
      EXPECT_TRUE(parsed.has_list_type());
      if (!original.GetParameters().empty()) {
        VerifyParsedMatchesType(parsed.list_type().elem_type(),
                                original.GetParameters()[0]);
      }
      break;
    case TypeKind::kMap:
      EXPECT_TRUE(parsed.has_map_type());
      if (!original.GetParameters().empty()) {
        VerifyParsedMatchesType(parsed.map_type().key_type(),
                                original.GetParameters()[0]);
      }
      if (original.GetParameters().size() > 1) {
        VerifyParsedMatchesType(parsed.map_type().value_type(),
                                original.GetParameters()[1]);
      }
      break;
    case TypeKind::kBoolWrapper:
    case TypeKind::kIntWrapper:
    case TypeKind::kUintWrapper:
    case TypeKind::kDoubleWrapper:
    case TypeKind::kStringWrapper:
    case TypeKind::kBytesWrapper:
      EXPECT_TRUE(parsed.has_wrapper());
      break;
    case TypeKind::kType:
      EXPECT_TRUE(parsed.has_type());
      if (!original.GetParameters().empty()) {
        VerifyParsedMatchesType(parsed.type(), original.GetParameters()[0]);
      }
      break;
    case TypeKind::kTypeParam:
      EXPECT_TRUE(parsed.has_type_param());
      break;
    default:
      EXPECT_TRUE(parsed.has_abstract_type());
      break;
  }
}

void VerifyTypesEqual(const Type& lhs, const Type& rhs) {
  EXPECT_EQ(lhs.kind(), rhs.kind());
  if (lhs.kind() != rhs.kind()) return;

  if (lhs.kind() == TypeKind::kOpaque || lhs.kind() == TypeKind::kStruct ||
      lhs.kind() == TypeKind::kTypeParam) {
    EXPECT_EQ(lhs.name(), rhs.name());
  }

  const auto& lhs_params = lhs.GetParameters();
  const auto& rhs_params = rhs.GetParameters();
  EXPECT_EQ(lhs_params.size(), rhs_params.size());
  if (lhs_params.size() == rhs_params.size()) {
    for (size_t i = 0; i < lhs_params.size(); ++i) {
      VerifyTypesEqual(lhs_params[i], rhs_params[i]);
    }
  }
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
          .type = TypeType(GetTestArena(), IntType{}),
          .expected_signature = "type<int>",
      },
      {
          .type = ListType(GetTestArena(), TypeParamType("A")),
          .expected_signature = "list<~A>",
      },
      {
          .type = ListType(GetTestArena(), TypeParamType("A<B")),
          .expected_signature = "list<~A\\<B>",
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
          .type = OpaqueType(GetTestArena(), "bar",
                             {FunctionType(GetTestArena(), TypeParamType("D"),
                                           {StringType{}, BoolType{}})}),
          .expected_signature = "bar<function<~D,string,bool>>",
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
          .type = BoolWrapperType{},
          .expected_signature = "bool_wrapper",
      },
      {
          .type = IntWrapperType{},
          .expected_signature = "int_wrapper",
      },
      {
          .type = UintWrapperType{},
          .expected_signature = "uint_wrapper",
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
  };
}

TEST(TypeSignatureTest, UnsupportedTypes) {
  EXPECT_THAT(common_internal::MakeTypeSignature(UnknownType{}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Type kind: *unknown* is not supported")));

  EXPECT_THAT(common_internal::MakeTypeSignature(ErrorType{}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Type kind: *error* is not supported")));
}

INSTANTIATE_TEST_SUITE_P(TypeIdTest, TypeSignatureTest,
                         ValuesIn(GetTypeSignatureTestCases()));

TEST_P(TypeSignatureTest, ParseTypeCheck) {
  const auto& param = GetParam();
  if (!param.expected_signature.empty() && param.expected_error.empty()) {
    auto parsed = ParseType(param.expected_signature, GetTestArena(),
                            *GetTestingDescriptorPool());
    ASSERT_THAT(parsed, ::absl_testing::IsOk());
    VerifyTypesEqual(*parsed, param.type);
  }
}

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
          .args = {BoolWrapperType{}},
          .expected_signature = "hello(bool_wrapper)",
      },
      {
          .args = {IntWrapperType{}},
          .expected_signature = "hello(int_wrapper)",
      },
      {
          .args = {UintWrapperType{}},
          .expected_signature = "hello(uint_wrapper)",
      },
      {
          .args = {MessageType(
              GetTestingDescriptorPool()->FindMessageTypeByName(
                  "cel.expr.conformance.proto3.TestAllTypes"))},
          .expected_signature =
              "hello(cel.expr.conformance.proto3.TestAllTypes)",
      },
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
          .function_name = "hello",
          .args = {OpaqueType(GetTestArena(), "bar",
                              {TypeParamType("dummy.type")})},
          .is_member = true,
          .expected_signature = R"(bar<~dummy\.type>.hello())",
      },
      {
          .function_name = "inspect",
          .args = {Type(TypeType(GetTestArena(), StringType{}))},
          .expected_signature = "inspect(type<string>)",
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

TEST(OverloadSignatureTest, MemberFunctionNoReceiverError) {
  auto signature = common_internal::MakeOverloadSignature("hello", {}, true);
  EXPECT_THAT(signature,
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Member function with no receiver")));
}

INSTANTIATE_TEST_SUITE_P(OverloadIdTest, OverloadSignatureTest,
                         ValuesIn(GetOverloadSignatureTestCases()));

TEST_P(OverloadSignatureTest, ExhaustiveFunctionParseCheck) {
  const auto& param = GetParam();
  if (!param.expected_signature.empty()) {
    auto parsed = ParseFunctionSignature(param.expected_signature);
    ASSERT_THAT(parsed, ::absl_testing::IsOk());
    EXPECT_EQ(parsed->function_name, param.function_name);
    EXPECT_EQ(parsed->is_member, param.is_member);
    EXPECT_TRUE(parsed->signature_type.has_function());
    const auto& func = parsed->signature_type.function();
    for (size_t i = 0; i < param.args.size(); ++i) {
      VerifyParsedMatchesType(func.arg_types()[i], param.args[i]);
    }
  }
}

TEST(ParseSignatureTest, ProtoParsing) {
  ASSERT_OK_AND_ASSIGN(
      auto t1, ParseType("int", GetTestArena(), *GetTestingDescriptorPool()));
  EXPECT_TRUE(t1.IsInt());

  ASSERT_OK_AND_ASSIGN(auto t2, ParseType("list<~A>", GetTestArena(),
                                          *GetTestingDescriptorPool()));
  EXPECT_TRUE(t2.IsList());

  ASSERT_OK_AND_ASSIGN(auto t3, ParseType(R"(~abc\)", GetTestArena(),
                                          *GetTestingDescriptorPool()));
  EXPECT_TRUE(t3.IsTypeParam());
  EXPECT_EQ(t3.GetTypeParam().name(), R"(abc\)");

  ASSERT_OK_AND_ASSIGN(auto w1,
                       ParseType("google.protobuf.BoolValue", GetTestArena(),
                                 *GetTestingDescriptorPool()));
  EXPECT_TRUE(w1.IsBoolWrapper());

  ASSERT_OK_AND_ASSIGN(auto w2,
                       ParseType("google.protobuf.Int64Value", GetTestArena(),
                                 *GetTestingDescriptorPool()));
  EXPECT_TRUE(w2.IsIntWrapper());

  ASSERT_OK_AND_ASSIGN(auto w3,
                       ParseType("google.protobuf.Int32Value", GetTestArena(),
                                 *GetTestingDescriptorPool()));
  EXPECT_TRUE(w3.IsIntWrapper());

  ASSERT_OK_AND_ASSIGN(auto w4,
                       ParseType("google.protobuf.UInt64Value", GetTestArena(),
                                 *GetTestingDescriptorPool()));
  EXPECT_TRUE(w4.IsUintWrapper());

  ASSERT_OK_AND_ASSIGN(auto w5,
                       ParseType("google.protobuf.UInt32Value", GetTestArena(),
                                 *GetTestingDescriptorPool()));
  EXPECT_TRUE(w5.IsUintWrapper());

  ASSERT_OK_AND_ASSIGN(auto w6,
                       ParseType("google.protobuf.DoubleValue", GetTestArena(),
                                 *GetTestingDescriptorPool()));
  EXPECT_TRUE(w6.IsDoubleWrapper());

  ASSERT_OK_AND_ASSIGN(auto w7,
                       ParseType("google.protobuf.FloatValue", GetTestArena(),
                                 *GetTestingDescriptorPool()));
  EXPECT_TRUE(w7.IsDoubleWrapper());

  ASSERT_OK_AND_ASSIGN(auto w8,
                       ParseType("google.protobuf.StringValue", GetTestArena(),
                                 *GetTestingDescriptorPool()));
  EXPECT_TRUE(w8.IsStringWrapper());

  ASSERT_OK_AND_ASSIGN(auto w9,
                       ParseType("google.protobuf.BytesValue", GetTestArena(),
                                 *GetTestingDescriptorPool()));
  EXPECT_TRUE(w9.IsBytesWrapper());

  ASSERT_OK_AND_ASSIGN(auto w10, ParseType("string_wrapper", GetTestArena(),
                                           *GetTestingDescriptorPool()));
  EXPECT_TRUE(w10.IsStringWrapper());

  ASSERT_OK_AND_ASSIGN(auto w11, ParseType("bytes_wrapper", GetTestArena(),
                                           *GetTestingDescriptorPool()));
  EXPECT_TRUE(w11.IsBytesWrapper());

  ASSERT_OK_AND_ASSIGN(auto gp_any,
                       ParseType("google.protobuf.Any", GetTestArena(),
                                 *GetTestingDescriptorPool()));
  EXPECT_TRUE(gp_any.IsAny());

  ASSERT_OK_AND_ASSIGN(auto gp_timestamp,
                       ParseType("google.protobuf.Timestamp", GetTestArena(),
                                 *GetTestingDescriptorPool()));
  EXPECT_TRUE(gp_timestamp.IsTimestamp());

  ASSERT_OK_AND_ASSIGN(auto gp_duration,
                       ParseType("google.protobuf.Duration", GetTestArena(),
                                 *GetTestingDescriptorPool()));
  EXPECT_TRUE(gp_duration.IsDuration());

  ASSERT_OK_AND_ASSIGN(auto gp_value,
                       ParseType("google.protobuf.Value", GetTestArena(),
                                 *GetTestingDescriptorPool()));
  EXPECT_TRUE(gp_value.IsDyn());

  ASSERT_OK_AND_ASSIGN(auto gp_list_value,
                       ParseType("google.protobuf.ListValue", GetTestArena(),
                                 *GetTestingDescriptorPool()));
  EXPECT_TRUE(gp_list_value.IsList());

  ASSERT_OK_AND_ASSIGN(auto gp_struct,
                       ParseType("google.protobuf.Struct", GetTestArena(),
                                 *GetTestingDescriptorPool()));
  EXPECT_TRUE(gp_struct.IsMap());

  // Legal whitespace handling tests
  ASSERT_OK_AND_ASSIGN(auto ws_type1,
                       ParseType("map <  int ,  string   > ", GetTestArena(),
                                 *GetTestingDescriptorPool()));
  EXPECT_TRUE(ws_type1.IsMap());

  ASSERT_OK_AND_ASSIGN(auto ws_type2,
                       ParseType("map\t<\nint\r,\tstring\n>\r", GetTestArena(),
                                 *GetTestingDescriptorPool()));
  EXPECT_TRUE(ws_type2.IsMap());
}

TEST(ParseSignatureTest, FunctionParsing) {
  ASSERT_OK_AND_ASSIGN(auto f1, ParseFunctionSignature("hello(string)"));
  EXPECT_TRUE(f1.signature_type.has_function());
  EXPECT_EQ(f1.signature_type.function().arg_types().size(), 1);

  // Legal whitespace handling tests
  ASSERT_OK_AND_ASSIGN(auto ws_func1,
                       ParseFunctionSignature("  hello  ( string    )  "));
  EXPECT_TRUE(ws_func1.signature_type.has_function());
  EXPECT_EQ(ws_func1.signature_type.function().arg_types().size(), 1);

  ASSERT_OK_AND_ASSIGN(auto ws_func2,
                       ParseFunctionSignature("\thello\n(\rstring\t)\n\r"));
  EXPECT_TRUE(ws_func2.signature_type.has_function());
  EXPECT_EQ(ws_func2.signature_type.function().arg_types().size(), 1);

  ASSERT_OK_AND_ASSIGN(auto f2, ParseFunctionSignature("a.b.c()"));
  EXPECT_TRUE(f2.is_member);
  EXPECT_EQ(f2.function_name, "c");
}

TEST(ParseSignatureTest, ParsingErrors) {
  // Mismatched template brackets and parentheses.
  EXPECT_THAT(
      ParseType("list<int>>", GetTestArena(), *GetTestingDescriptorPool()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("mismatched brackets")));
  EXPECT_THAT(
      ParseType("list<list<int>", GetTestArena(), *GetTestingDescriptorPool()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("mismatched brackets")));
  EXPECT_THAT(ParseType("list><", GetTestArena(), *GetTestingDescriptorPool()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("mismatched brackets")));
  EXPECT_THAT(ParseFunctionSignature("hello(list<int>>)"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("mismatched brackets")));
  EXPECT_THAT(ParseFunctionSignature("hello(list<list<int>)"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("mismatched brackets")));
  EXPECT_THAT(ParseFunctionSignature("foo<bar"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("mismatched brackets")));
  EXPECT_THAT(ParseFunctionSignature("foo<bar.baz()"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("mismatched brackets")));

  // Parameter count validations for list and map types.
  EXPECT_THAT(ParseType("list<int,string>", GetTestArena(),
                        *GetTestingDescriptorPool()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("list expects at most 1 parameter")));
  EXPECT_THAT(
      ParseType("map<int>", GetTestArena(), *GetTestingDescriptorPool()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("map expects 0 or 2 parameters")));
  EXPECT_THAT(ParseType("map<int,string,dyn>", GetTestArena(),
                        *GetTestingDescriptorPool()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("map expects 0 or 2 parameters")));

  // Enforcing valid function and identifier names.
  EXPECT_THAT(ParseFunctionSignature("()"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("empty function name")));
  EXPECT_THAT(ParseFunctionSignature("string.()"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("empty function name")));

  // Missing closing operators and boundary checks.
  EXPECT_THAT(
      ParseType("list<int>foo", GetTestArena(), *GetTestingDescriptorPool()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("missing closing >")));

  EXPECT_THAT(ParseFunctionSignature("hello>(string)"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("mismatched brackets")));
  EXPECT_THAT(
      ParseType("list<<int>", GetTestArena(), *GetTestingDescriptorPool()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("mismatched brackets")));

  EXPECT_THAT(ParseType("map<int, string\\>", GetTestArena(),
                        *GetTestingDescriptorPool()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("mismatched brackets")));

  EXPECT_THAT(ParseType("map int, string>", GetTestArena(),
                        *GetTestingDescriptorPool()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("mismatched brackets")));

  EXPECT_THAT(ParseType("list<int, string >", GetTestArena(),
                        *GetTestingDescriptorPool()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid type signature")));

  EXPECT_THAT(ParseFunctionSignature("a..b.c()"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid type signature")));
  EXPECT_THAT(
      ParseType("list<int,>", GetTestArena(), *GetTestingDescriptorPool()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Empty type signature")));

  EXPECT_THAT(
      ParseType("~list<int>", GetTestArena(), *GetTestingDescriptorPool()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Invalid type signature")));

  // Checks that builtin types cannot have type parameters.
  EXPECT_THAT(
      ParseType("int<int>", GetTestArena(), *GetTestingDescriptorPool()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("cannot have type parameters")));
}

TEST(ParseSignatureTest, MessageTypeWithParamsError) {
  EXPECT_THAT(ParseType("cel.expr.conformance.proto3.TestAllTypes<int>",
                        GetTestArena(), *GetTestingDescriptorPool()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("cannot have type parameters")));
}

TEST(ParseSignatureTest, MissingClosingParenthesisError) {
  EXPECT_THAT(ParseFunctionSignature("hello(string"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid function signature")));
  EXPECT_THAT(ParseFunctionSignature("hello)"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid function signature")));
}

TEST(ParseSignatureTest, NestedDotsNonMember) {
  auto f1 = ParseFunctionSignature(
      "my_opaque<cel.expr.conformance.proto3.TestAllTypes>()");
  ASSERT_THAT(f1, ::absl_testing::IsOk());
  EXPECT_FALSE(f1->is_member);
  EXPECT_EQ(f1->function_name,
            "my_opaque<cel.expr.conformance.proto3.TestAllTypes>");
}

TEST(ParseSignatureTest, OverlyComplexSignatures) {
  auto t1 = ParseType("map<list<~A\\<B\\>>,map<string,list<~C>>>",
                      GetTestArena(), *GetTestingDescriptorPool());
  ASSERT_THAT(t1, ::absl_testing::IsOk());
  EXPECT_TRUE(t1->IsMap());

  auto t2 = ParseType(R"(~abc\\)", GetTestArena(), *GetTestingDescriptorPool());
  ASSERT_THAT(t2, ::absl_testing::IsOk());
  EXPECT_TRUE(t2->IsTypeParam());
  EXPECT_EQ(t2->GetTypeParam().name(), R"(abc\)");

  auto t3 =
      ParseType(R"(~abc\\\\)", GetTestArena(), *GetTestingDescriptorPool());
  ASSERT_THAT(t3, ::absl_testing::IsOk());
  EXPECT_TRUE(t3->IsTypeParam());
  EXPECT_EQ(t3->GetTypeParam().name(), R"(abc\\)");

  auto f1 = ParseFunctionSignature(
      "bar<function<int,list<~A>>,map<string,dyn>>.func(string)");
  ASSERT_THAT(f1, ::absl_testing::IsOk());
  EXPECT_TRUE(f1->is_member);
  EXPECT_EQ(f1->function_name, "func");
  EXPECT_TRUE(f1->signature_type.has_function());
  EXPECT_EQ(f1->signature_type.function().arg_types().size(), 2);
}

TEST(ParseSignatureTest, EmptyOrWhitespaceErrors) {
  EXPECT_THAT(ParseType("", GetTestArena(), *GetTestingDescriptorPool()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Empty type signature")));
  EXPECT_THAT(ParseFunctionSignature(""),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Empty function signature")));
  EXPECT_THAT(ParseType("list<map<int,,dyn>>", GetTestArena(),
                        *GetTestingDescriptorPool()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Empty type signature")));
}

}  // namespace
}  // namespace cel::common_internal
