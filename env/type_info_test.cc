// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "env/type_info.h"

#include <string>
#include <vector>

#include "common/type.h"
#include "common/type_proto.h"
#include "env/config.h"
#include "internal/proto_matchers.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/text_format.h"

namespace cel {
namespace {

using absl_testing::IsOk;
using testing::ValuesIn;

struct TestCase {
  Config::TypeInfo type_info;
  std::string expected_type_pb;
};

using TypeInfoTest = testing::TestWithParam<TestCase>;

TEST_P(TypeInfoTest, TypeInfo) {
  const TestCase& param = GetParam();
  cel::expr::Type expected_type_pb;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(param.expected_type_pb,
                                                  &expected_type_pb));

  google::protobuf::Arena arena;
  const google::protobuf::DescriptorPool* descriptor_pool =
      cel::internal::GetTestingDescriptorPool();
  ASSERT_OK_AND_ASSIGN(
      cel::Type actual_type,
      cel::TypeInfoToType(param.type_info, descriptor_pool, &arena));

  cel::expr::Type actual_type_pb;
  ASSERT_THAT(cel::TypeToProto(actual_type, &actual_type_pb), IsOk());
  EXPECT_THAT(actual_type_pb,
              cel::internal::test::EqualsProto(expected_type_pb));
}

std::vector<TestCase> GetTestCases() {
  return {
      TestCase{
          .type_info = {.name = "int"},
          .expected_type_pb = "primitive: INT64",
      },
      TestCase{
          .type_info = {.name = "list",
                        .params = {Config::TypeInfo{.name = "int"}}},
          .expected_type_pb = "list_type { elem_type { primitive: INT64 } }",
      },
      TestCase{
          .type_info = {.name = "list"},
          .expected_type_pb = "list_type { elem_type { dyn {} }}",
      },
      TestCase{
          .type_info = {.name = "map",
                        .params = {Config::TypeInfo{.name = "string"},
                                   Config::TypeInfo{.name = "int"}}},
          .expected_type_pb = "map_type { key_type { primitive: STRING } "
                              "value_type { primitive: INT64 }}",
      },
      TestCase{
          .type_info = {.name = "cel.expr.conformance.proto2.TestAllTypes"},
          .expected_type_pb =
              "message_type: 'cel.expr.conformance.proto2.TestAllTypes'",
      },
      TestCase{
          .type_info = {.name = "A",
                        .params = {Config::TypeInfo{.name = "B",
                                                    .is_type_param = true}}},
          .expected_type_pb =
              "abstract_type { name: 'A' parameter_types { type_param: 'B' } }",
      },
      TestCase{
          .type_info = {.name = "any"},
          .expected_type_pb = "well_known: ANY",
      },
      TestCase{
          .type_info = {.name = "timestamp"},
          .expected_type_pb = "well_known: TIMESTAMP",
      },
      TestCase{
          .type_info = {.name = "google.protobuf.DoubleValue"},
          .expected_type_pb = "wrapper: DOUBLE",
      },
      TestCase{
          .type_info = {.name = "wrapper<double>"},
          .expected_type_pb = "wrapper: DOUBLE",
      },
      TestCase{
          .type_info = {.name = "type",
                        .params = {Config::TypeInfo{.name = "duration"}}},
          .expected_type_pb = "type: { well_known: DURATION }",
      },
      TestCase{
          .type_info = {.name = "parameterized",
                        .params = {{.name = "A", .is_type_param = true},
                                   {.name = "double"}}},
          .expected_type_pb = "abstract_type { name: 'parameterized' "
                              "parameter_types { type_param: 'A' } "
                              "parameter_types { primitive: DOUBLE } }",
      },
  };
}

INSTANTIATE_TEST_SUITE_P(TypeInfoTest, TypeInfoTest, ValuesIn(GetTestCases()));

}  // namespace
}  // namespace cel
