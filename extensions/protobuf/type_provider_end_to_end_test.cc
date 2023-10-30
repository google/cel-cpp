// Copyright 2023 Google LLC
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

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/descriptor.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "base/handle.h"
#include "base/memory.h"
#include "base/value.h"
#include "base/values/bool_value.h"
#include "base/values/error_value.h"
#include "extensions/protobuf/enum_adapter.h"
#include "extensions/protobuf/memory_manager.h"
#include "extensions/protobuf/runtime_adapter.h"
#include "extensions/protobuf/type_provider.h"
#include "internal/testing.h"
#include "parser/parser.h"
#include "runtime/activation.h"
#include "runtime/managed_value_factory.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/generated_enum_reflection.h"
#include "google/protobuf/message.h"

namespace cel::extensions {
namespace {

using ::google::api::expr::v1alpha1::ParsedExpr;
using ::google::api::expr::parser::Parse;
using ::google::api::expr::test::v1::proto3::TestAllTypes;
using testing::HasSubstr;
using cel::internal::StatusIs;

struct TestCase {
  std::string name;
  std::string expr;
  absl::StatusOr<bool> result = {true};
};

enum class MemoryManagerKind { kGlobal, kProto };

class TypeProviderSetGetTest
    : public ::testing::TestWithParam<std::tuple<TestCase, MemoryManagerKind>> {
 public:
  TypeProviderSetGetTest() : proto_memory_manager_(&arena_) {}

  MemoryManagerKind GetMemoryManagerKind() { return std::get<1>(GetParam()); }
  MemoryManager& GetMemoryManager() {
    return GetMemoryManagerKind() == MemoryManagerKind::kProto
               ? proto_memory_manager_
               : MemoryManager::Global();
  }
  const TestCase& GetTestCase() { return std::get<0>(GetParam()); }

 private:
  google::protobuf::Arena arena_;
  ProtoMemoryManager proto_memory_manager_;
  google::protobuf::SimpleDescriptorDatabase descriptor_database_;
};

TEST_P(TypeProviderSetGetTest, Generated) {
  RuntimeOptions options;
  options.container = "google.api.expr.test.v1.proto3";
  ASSERT_OK_AND_ASSIGN(RuntimeBuilder builder,
                       CreateStandardRuntimeBuilder(options));

  google::protobuf::LinkMessageReflection<
      google::api::expr::test::v1::proto3::TestAllTypes>();
  builder.type_registry().AddTypeProvider(
      std::make_unique<ProtoTypeProvider>());

  ASSERT_OK(RegisterProtobufEnum(
      builder.type_registry(),
      google::protobuf::GetEnumDescriptor<TestAllTypes::NestedEnum>()));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<const Runtime> runtime,
                       std::move(builder).Build());

  const TestCase& test_case = GetTestCase();
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse(test_case.expr));

  ASSERT_OK_AND_ASSIGN(auto program, ProtobufRuntimeAdapter::CreateProgram(
                                         *runtime, parsed_expr));

  ManagedValueFactory factory(program->GetTypeProvider(), GetMemoryManager());
  Activation activation;

  ASSERT_OK_AND_ASSIGN(Handle<Value> result,
                       program->Evaluate(activation, factory.get()));

  if (test_case.result.ok()) {
    ASSERT_TRUE(result->Is<BoolValue>()) << result->DebugString();
    EXPECT_TRUE(result->As<BoolValue>().NativeValue());
  } else {
    ASSERT_TRUE(result->Is<ErrorValue>()) << result->DebugString();
    EXPECT_THAT(result->As<ErrorValue>().NativeValue(),
                StatusIs(test_case.result.status().code(),
                         HasSubstr(test_case.result.status().message())));
  }
}

TEST_P(TypeProviderSetGetTest, Custom) {
  google::protobuf::SimpleDescriptorDatabase descriptor_database;
  google::protobuf::FileDescriptorProto fd_proto;
  TestAllTypes::descriptor()->file()->CopyTo(&fd_proto);
  ASSERT_TRUE(descriptor_database.Add(fd_proto));
  for (int i = 0; i < TestAllTypes::descriptor()->file()->dependency_count();
       ++i) {
    fd_proto.Clear();
    TestAllTypes::descriptor()->file()->dependency(i)->CopyTo(&fd_proto);
    ASSERT_TRUE(descriptor_database.Add(fd_proto));
  }
  google::protobuf::DescriptorPool descriptor_pool(&descriptor_database);
  google::protobuf::DynamicMessageFactory message_factory;
  google::protobuf::FileDescriptorProto file_descriptor_proto;

  RuntimeOptions options;
  options.container = "google.api.expr.test.v1.proto3";

  ASSERT_OK_AND_ASSIGN(RuntimeBuilder builder,
                       CreateStandardRuntimeBuilder(options));

  builder.type_registry().AddTypeProvider(
      std::make_unique<ProtoTypeProvider>(&descriptor_pool, &message_factory));
  ASSERT_OK(RegisterProtobufEnum(
      builder.type_registry(),
      google::protobuf::GetEnumDescriptor<TestAllTypes::NestedEnum>()));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<const Runtime> runtime,
                       std::move(builder).Build());

  const TestCase& test_case = GetTestCase();
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse(test_case.expr));

  ASSERT_OK_AND_ASSIGN(auto program, ProtobufRuntimeAdapter::CreateProgram(
                                         *runtime, parsed_expr));

  ManagedValueFactory factory(program->GetTypeProvider(), GetMemoryManager());
  Activation activation;

  ASSERT_OK_AND_ASSIGN(Handle<Value> result,
                       program->Evaluate(activation, factory.get()));

  if (test_case.result.ok()) {
    ASSERT_TRUE(result->Is<BoolValue>()) << result->DebugString();
    EXPECT_TRUE(result->As<BoolValue>().NativeValue());
  } else {
    ASSERT_TRUE(result->Is<ErrorValue>()) << result->DebugString();
    EXPECT_THAT(result->As<ErrorValue>().NativeValue(),
                StatusIs(test_case.result.status().code(),
                         HasSubstr(test_case.result.status().message())));
  }
}

std::string TypeProviderSetGetTestName(
    const testing::TestParamInfo<std::tuple<TestCase, MemoryManagerKind>>&
        info) {
  const TestCase& test_case = std::get<0>(info.param);
  MemoryManagerKind memory_manager = std::get<1>(info.param);

  return absl::StrCat(
      test_case.name, "_",
      memory_manager == MemoryManagerKind::kGlobal ? "global" : "arena");
}

INSTANTIATE_TEST_SUITE_P(
    Basic, TypeProviderSetGetTest,
    ::testing::Combine(
        ::testing::ValuesIn<TestCase>({
            {"single_int64",
             R"cel(
              TestAllTypes{
                single_int64: -25
              }.single_int64 == -25
             )cel"},
            {"single_int32",
             R"cel(
              TestAllTypes{
                single_int32: -26
              }.single_int32 == -26
             )cel"},
            {"single_int32_overflow",
             R"cel(
              TestAllTypes{
                single_int32: 3 * 1000 * 1000 * 1000
              }
             )cel",
             absl::InvalidArgumentError("int64 to int32_t overflow")},
            {"single_uint64",
             R"cel(
              TestAllTypes{
                single_uint64: 25u
              }.single_uint64 == 25u
             )cel"},
            {"single_uint32",
             R"cel(
              TestAllTypes{
                single_uint32: 26u
              }.single_uint32 == 26u
             )cel"},
            {"single_uint32_overflow",
             R"cel(
              TestAllTypes{
                single_uint32: 5u * 1000u * 1000u * 1000u
              }
             )cel",
             absl::InvalidArgumentError("uint64 to uint32_t overflow")},
            {"single_sint32",
             R"cel(
              TestAllTypes{
                single_sint32: 27
              }.single_sint32 == 27
             )cel"},
            {"single_sint64",
             R"cel(
              TestAllTypes{
                single_sint64: 28
              }.single_sint64 == 28
             )cel"},
            {"single_fixed32",
             R"cel(
              TestAllTypes{
                single_fixed32: 29u
              }.single_fixed32 == 29u
             )cel"},
            {"single_fixed64",
             R"cel(
              TestAllTypes{
                single_fixed64: 30u
              }.single_fixed64 == 30u
             )cel"},
            {"single_sfixed32",
             R"cel(
              TestAllTypes{
                single_sfixed32: -31
              }.single_sfixed32 == -31
             )cel"},
            {"single_sfixed64",
             R"cel(
              TestAllTypes{
                single_sfixed64: -32
              }.single_sfixed64 == -32
             )cel"},
            {"single_float",
             R"cel(
              TestAllTypes{
                single_float: -8.25
              }.single_float == -8.25
             )cel"},
            {"single_float_precision_overflow",
             R"cel(
              TestAllTypes{
                single_float: -33.3
              }.single_float == -33.3
             )cel",
             absl::InvalidArgumentError("double to float overflow")},
            {"single_float_range_overflow",
             R"cel(
              TestAllTypes{
                single_float: 3.4028238e38
              }.single_float == 3.4028238e38
             )cel",
             absl::InvalidArgumentError("double to float overflow")},
            {"single_double",
             R"cel(
              TestAllTypes{
                single_double: 34.5
              }.single_double == 34.5
             )cel"},
            {"single_bool",
             R"cel(
              TestAllTypes{
                single_bool: false
              }.single_bool == false
             )cel"},
            {"single_string",
             R"cel(
              TestAllTypes{
                single_string: "abcd"
              }.single_string == "abcd"
             )cel"},
            {"single_bytes",
             R"cel(
              TestAllTypes{
                single_bytes: b"bytes"
              }.single_bytes == b"bytes"
             )cel"},
            {"single_primitive_wrong_type",
             R"cel(
              TestAllTypes{
                single_int64: "25"
              }.single_int64
             )cel",
             absl::InvalidArgumentError(
                 "type conversion error from int to string")},
            {"no_such_field",
             R"cel(
              TestAllTypes{
                single_int64: 32
              }.unknown_field
             )cel",
             absl::NotFoundError("no_such_field : unknown_field")},
        }),
        ::testing::Values(MemoryManagerKind::kGlobal,
                          MemoryManagerKind::kProto)),
    TypeProviderSetGetTestName);

INSTANTIATE_TEST_SUITE_P(
    WellKnown, TypeProviderSetGetTest,
    ::testing::Combine(::testing::ValuesIn<TestCase>({
                           {"single_any",
                            R"cel(
                              TestAllTypes{
                                single_any: TestAllTypes{
                                  single_int64: 50
                                }
                              }.single_any.single_int64 == 50
                            )cel"},
                           {"single_any_primitive",
                            R"cel(
                              TestAllTypes{
                                single_any: int(1234)
                              }.single_any == 1234
                            )cel"},
                           {"single_duration",
                            R"cel(
                              TestAllTypes{
                                single_duration: duration("1h")
                              }.single_duration == duration("1h")
                            )cel"},
                           {"single_timestamp",
                            R"cel(
                              TestAllTypes{
                                single_timestamp: timestamp("2001-01-01T12:00:00Z")
                              }.single_timestamp == timestamp("2001-01-01T12:00:00Z")
                            )cel"},
                           {"single_value_string",
                            R"cel(
                              TestAllTypes{
                                single_value: "test"
                              }.single_value == "test"
                            )cel"},
                           {"single_int64_wrapper",
                            R"cel(
                              TestAllTypes{
                                single_int64_wrapper: 12
                              }.single_int64_wrapper == 12
                            )cel"},
                           {"single_int32_wrapper",
                            R"cel(
                              TestAllTypes{
                                single_int32_wrapper: 70
                              }.single_int32_wrapper == 70
                            )cel"},
                           {"single_double_wrapper",
                            R"cel(
                              TestAllTypes{
                                single_double_wrapper: 3.1
                              }.single_double_wrapper == 3.1
                            )cel"},
                           {"single_float_wrapper",
                            R"cel(
                              TestAllTypes{
                                single_float_wrapper: 1.125
                              }.single_float_wrapper == 1.125
                            )cel"},
                           {"single_uint64_wrapper",
                            R"cel(
                              TestAllTypes{
                                single_uint64_wrapper: 13u
                              }.single_uint64_wrapper == 13u
                            )cel"},
                           {"single_uint32_wrapper",
                            R"cel(
                              TestAllTypes{
                                single_uint32_wrapper: 71u
                              }.single_uint32_wrapper == 71u
                            )cel"},
                           {"single_string_wrapper",
                            R"cel(
                              TestAllTypes{
                                single_string_wrapper: "123"
                              }.single_string_wrapper == "123"
                            )cel"},
                           {"single_bytes_wrapper",
                            R"cel(
                              TestAllTypes{
                                single_bytes_wrapper: b"321"
                              }.single_bytes_wrapper == b"321"
                            )cel"},
                           {"single_null_value",
                            R"cel(
                              TestAllTypes{
                                null_value: null
                              }.null_value == null
                            )cel"},
                           {"optional_null_value",
                            R"cel(
                              TestAllTypes{
                                optional_null_value: null
                              }.optional_null_value == null
                            )cel"},
                           // TODO(uncreated-issue/30): crashes for arena allocator
                           // {"list_value",
                           //                       R"cel(
                           //                         TestAllTypes{
                           //                           list_value: [1, 2, 3]
                           //                         }.list_value == [1, 2, 3]
                           //                       )cel"},
                       }),
                       ::testing::Values(MemoryManagerKind::kGlobal,
                                         MemoryManagerKind::kProto)),
    TypeProviderSetGetTestName);

INSTANTIATE_TEST_SUITE_P(
    Repeated, TypeProviderSetGetTest,
    ::testing::Combine(::testing::ValuesIn<TestCase>({
                           {"repeated_int64",
                            R"cel(
                              TestAllTypes{
                                repeated_int64: [0, -25]
                              }.repeated_int64[1] == -25
                            )cel"},
                           {"repeated_int32",
                            R"cel(
                              TestAllTypes{
                                repeated_int32: [0, -26]
                              }.repeated_int32[1] == -26
                            )cel"},
                           {"repeated_uint64",
                            R"cel(
                              TestAllTypes{
                                repeated_uint64: [0u, 25u]
                              }.repeated_uint64[1] == 25u
                            )cel"},
                           {"repeated_uint32",
                            R"cel(
                              TestAllTypes{
                                repeated_uint32: [0u, 26u]
                              }.repeated_uint32[1] == 26u
                            )cel"},
                           {"repeated_sint32",
                            R"cel(
                              TestAllTypes{
                                repeated_sint32: [0, 27]
                              }.repeated_sint32[1] == 27
                            )cel"},
                           {"repeated_sint64",
                            R"cel(
                              TestAllTypes{
                                repeated_sint64: [0, 28]
                              }.repeated_sint64[1] == 28
                            )cel"},
                           {"repeated_fixed32",
                            R"cel(
                              TestAllTypes{
                                repeated_fixed32: [0u, 29u]
                              }.repeated_fixed32[1] == 29u
                            )cel"},
                           {"repeated_fixed64",
                            R"cel(
                              TestAllTypes{
                                repeated_fixed64: [0u, 30u]
                              }.repeated_fixed64[1] == 30u
                            )cel"},
                           {"repeated_sfixed32",
                            R"cel(
                              TestAllTypes{
                                repeated_sfixed32: [0, -31]
                              }.repeated_sfixed32[1] == -31
                            )cel"},
                           {"repeated_sfixed64",
                            R"cel(
                              TestAllTypes{
                                repeated_sfixed64: [0, -32]
                              }.repeated_sfixed64[1] == -32
                            )cel"},
                           {"repeated_float",
                            R"cel(
                              TestAllTypes{
                                repeated_float: [0.0, -8.25]
                              }.repeated_float[1] == -8.25
                            )cel"},
                           {"repeated_double",
                            R"cel(
                              TestAllTypes{
                                repeated_double: [0.0, 34.5]
                              }.repeated_double[1] == 34.5
                            )cel"},
                           {"repeated_bool",
                            R"cel(
                              TestAllTypes{
                                repeated_bool: [true, false]
                              }.repeated_bool[1] == false
                            )cel"},
                           {"repeated_string",
                            R"cel(
                              TestAllTypes{
                                repeated_string: ["", "abcd"]
                              }.repeated_string[1] == "abcd"
                            )cel"},
                           {"repeated_bytes",
                            R"cel(
                              TestAllTypes{
                                repeated_bytes: [b"bytes", b"bytes2"]
                              }.repeated_bytes[1] == b"bytes2"
                            )cel"},
                       }),
                       ::testing::Values(MemoryManagerKind::kGlobal,
                                         MemoryManagerKind::kProto)),
    TypeProviderSetGetTestName);

INSTANTIATE_TEST_SUITE_P(
    UserDefined, TypeProviderSetGetTest,
    ::testing::Combine(
        ::testing::ValuesIn<TestCase>({
            {"standalone_message",
             R"cel(
              TestAllTypes{
                standalone_message: TestAllTypes.NestedMessage{bb: 60}
              }.standalone_message.bb == 60
             )cel"},
            {"oneof_single_nested_message",
             R"cel(
              TestAllTypes{
                single_nested_message: TestAllTypes.NestedMessage{bb: 61}
              }.single_nested_message.bb == 61
             )cel"},
            // TODO(uncreated-issue/30): bad destructor call with Arena
            // {"repeated_nested_message",
            // R"cel(
            //   TestAllTypes{
            //     repeated_nested_message: [
            //       TestAllTypes.NestedMessage{bb: 62},
            //       TestAllTypes.NestedMessage{bb: 63}
            //     ]
            //   }.repeated_nested_message[1].bb == 63
            // )cel"},
            {"nested_test_all_types",
             R"cel(
              NestedTestAllTypes{
                child: NestedTestAllTypes{
                  child: NestedTestAllTypes{
                    child: NestedTestAllTypes{
                      payload: TestAllTypes{
                        single_int64: 42
                        }
                      }
                    }
                  }
              }.child.child.child.payload.single_int64 == 42
             )cel"},
            // TODO(uncreated-issue/30): field access returns strongly typed enum
            // equality needs to be updated to support.
            //  {"standalone_enum",
            //   R"cel(
            //     TestAllTypes{
            //       standalone_enum: TestAllTypes.NestedEnum.FOO
            //     }.standalone_enum == TestAllTypes.NestedEnum.FOO
            //   )cel"},
            //  {"oneof_single_nested_enum",
            //   R"cel(
            //     TestAllTypes{
            //       single_nested_enum: TestAllTypes.NestedEnum.BAR
            //     }.single_nested_enum == TestAllTypes.NestedEnum.BAR
            //   )cel"},
            //  {"repeated_nested_enum",
            //   R"cel(
            //     TestAllTypes{
            //       repeated_nested_enum: [
            //         TestAllTypes.NestedEnum.FOO,
            //         TestAllTypes.NestedEnum.BAZ]
            //     }.single_nested_enum == [
            //       TestAllTypes.NestedEnum.FOO,
            //       TestAllTypes.NestedEnum.BAZ]
            //   )cel"},
        }),
        ::testing::Values(MemoryManagerKind::kGlobal,
                          MemoryManagerKind::kProto)),
    TypeProviderSetGetTestName);

}  // namespace
}  // namespace cel::extensions
