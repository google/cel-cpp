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

#include "extensions/protobuf/bind_proto_to_activation.h"

#include "google/protobuf/wrappers.pb.h"
#include "absl/status/status.h"
#include "absl/types/optional.h"
#include "base/handle.h"
#include "base/memory.h"
#include "base/value.h"
#include "base/values/int_value.h"
#include "base/values/list_value.h"
#include "base/values/map_value.h"
#include "extensions/protobuf/memory_manager.h"
#include "extensions/protobuf/type_provider.h"
#include "internal/testing.h"
#include "runtime/activation.h"
#include "runtime/managed_value_factory.h"
#include "proto/test/v1/proto2/test_all_types.pb.h"
#include "google/protobuf/arena.h"

namespace cel::extensions {
namespace {

using ::google::api::expr::test::v1::proto2::TestAllTypes;
using testing::HasSubstr;
using testing::Optional;
using cel::internal::IsOkAndHolds;
using cel::internal::StatusIs;

enum class MemoryManagerOption { kGlobal, kArena };

class BindProtoToActivationTest
    : public ::testing::TestWithParam<MemoryManagerOption> {
 public:
  BindProtoToActivationTest() : proto_memory_manager_(&arena_) {}
  cel::MemoryManager& memory_manager() {
    return GetParam() == MemoryManagerOption::kGlobal ? MemoryManager::Global()
                                                      : proto_memory_manager_;
  }

 private:
  google::protobuf::Arena arena_;
  ProtoMemoryManager proto_memory_manager_;
};

MATCHER_P(IsIntValue, value, "") {
  const Handle<Value>& handle = arg;

  return handle->Is<IntValue>() &&
         handle->As<IntValue>().NativeValue() == value;
}

TEST_P(BindProtoToActivationTest, BindProtoToActivation) {
  ProtoTypeProvider provider;
  ManagedValueFactory value_factory(provider, memory_manager());
  TestAllTypes test_all_types;
  test_all_types.set_single_int64(123);
  Activation activation;

  ASSERT_OK(
      BindProtoToActivation(test_all_types, value_factory.get(), activation));

  EXPECT_THAT(activation.FindVariable(value_factory.get(), "single_int64"),
              IsOkAndHolds(Optional(IsIntValue(123))));
}

TEST_P(BindProtoToActivationTest, BindProtoToActivationWktUnsupported) {
  ProtoTypeProvider provider;
  ManagedValueFactory value_factory(provider, memory_manager());
  google::protobuf::Int64Value int64_value;
  int64_value.set_value(123);
  Activation activation;

  EXPECT_THAT(
      BindProtoToActivation(int64_value, value_factory.get(), activation),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("google.protobuf.Int64Value")));
}

TEST_P(BindProtoToActivationTest, BindProtoToActivationSkip) {
  ProtoTypeProvider provider;
  ManagedValueFactory value_factory(provider, memory_manager());
  TestAllTypes test_all_types;
  test_all_types.set_single_int64(123);
  Activation activation;

  ASSERT_OK(BindProtoToActivation(test_all_types, value_factory.get(),
                                  activation,
                                  BindProtoUnsetFieldBehavior::kSkip));

  EXPECT_THAT(activation.FindVariable(value_factory.get(), "single_int32"),
              IsOkAndHolds(absl::nullopt));
  EXPECT_THAT(activation.FindVariable(value_factory.get(), "single_sint32"),
              IsOkAndHolds(absl::nullopt));
}

TEST_P(BindProtoToActivationTest, BindProtoToActivationDefault) {
  ProtoTypeProvider provider;
  ManagedValueFactory value_factory(provider, memory_manager());
  TestAllTypes test_all_types;
  test_all_types.set_single_int64(123);
  Activation activation;

  ASSERT_OK(
      BindProtoToActivation(test_all_types, value_factory.get(), activation,
                            BindProtoUnsetFieldBehavior::kBindDefaultValue));

  // from test_all_types.proto
  // optional int32_t single_int32 = 1 [default = -32];
  EXPECT_THAT(activation.FindVariable(value_factory.get(), "single_int32"),
              IsOkAndHolds(Optional(IsIntValue(-32))));
  EXPECT_THAT(activation.FindVariable(value_factory.get(), "single_sint32"),
              IsOkAndHolds(Optional(IsIntValue(0))));
}

MATCHER_P(IsListValueOfSize, size, "") {
  const Handle<Value>& handle = arg;

  return handle->Is<ListValue>() && handle->As<ListValue>().Size() == size;
}

TEST_P(BindProtoToActivationTest, BindProtoToActivationRepeated) {
  ProtoTypeProvider provider;
  ManagedValueFactory value_factory(provider, memory_manager());
  TestAllTypes test_all_types;
  test_all_types.add_repeated_int64(123);
  test_all_types.add_repeated_int64(456);
  test_all_types.add_repeated_int64(789);

  Activation activation;

  ASSERT_OK(
      BindProtoToActivation(test_all_types, value_factory.get(), activation));

  EXPECT_THAT(activation.FindVariable(value_factory.get(), "repeated_int64"),
              IsOkAndHolds(Optional(IsListValueOfSize(3))));
}

TEST_P(BindProtoToActivationTest, BindProtoToActivationRepeatedEmpty) {
  ProtoTypeProvider provider;
  ManagedValueFactory value_factory(provider, memory_manager());
  TestAllTypes test_all_types;
  test_all_types.set_single_int64(123);
  Activation activation;

  ASSERT_OK(
      BindProtoToActivation(test_all_types, value_factory.get(), activation));

  EXPECT_THAT(activation.FindVariable(value_factory.get(), "repeated_int32"),
              IsOkAndHolds(Optional(IsListValueOfSize(0))));
}

TEST_P(BindProtoToActivationTest, BindProtoToActivationRepeatedComplex) {
  ProtoTypeProvider provider;
  ManagedValueFactory value_factory(provider, memory_manager());
  TestAllTypes test_all_types;
  auto* nested = test_all_types.add_repeated_nested_message();
  nested->set_bb(123);
  nested = test_all_types.add_repeated_nested_message();
  nested->set_bb(456);
  nested = test_all_types.add_repeated_nested_message();
  nested->set_bb(789);
  Activation activation;

  ASSERT_OK(
      BindProtoToActivation(test_all_types, value_factory.get(), activation));

  EXPECT_THAT(
      activation.FindVariable(value_factory.get(), "repeated_nested_message"),
      IsOkAndHolds(Optional(IsListValueOfSize(3))));
}

MATCHER_P(IsMapValueOfSize, size, "") {
  const Handle<Value>& handle = arg;

  return handle->Is<MapValue>() && handle->As<MapValue>().size() == size;
}

TEST_P(BindProtoToActivationTest, BindProtoToActivationMap) {
  ProtoTypeProvider provider;
  ManagedValueFactory value_factory(provider, memory_manager());
  TestAllTypes test_all_types;
  (*test_all_types.mutable_map_int64_int64())[1] = 2;
  (*test_all_types.mutable_map_int64_int64())[2] = 4;

  Activation activation;

  ASSERT_OK(
      BindProtoToActivation(test_all_types, value_factory.get(), activation));

  EXPECT_THAT(activation.FindVariable(value_factory.get(), "map_int64_int64"),
              IsOkAndHolds(Optional(IsMapValueOfSize(2))));
}

TEST_P(BindProtoToActivationTest, BindProtoToActivationMapEmpty) {
  ProtoTypeProvider provider;
  ManagedValueFactory value_factory(provider, memory_manager());
  TestAllTypes test_all_types;
  test_all_types.set_single_int64(123);
  Activation activation;

  ASSERT_OK(
      BindProtoToActivation(test_all_types, value_factory.get(), activation));

  EXPECT_THAT(activation.FindVariable(value_factory.get(), "map_int32_int32"),
              IsOkAndHolds(Optional(IsMapValueOfSize(0))));
}

TEST_P(BindProtoToActivationTest, BindProtoToActivationMapComplex) {
  ProtoTypeProvider provider;
  ManagedValueFactory value_factory(provider, memory_manager());
  TestAllTypes test_all_types;
  TestAllTypes::NestedMessage value;
  value.set_bb(42);
  (*test_all_types.mutable_map_int64_message())[1] = value;
  (*test_all_types.mutable_map_int64_message())[2] = value;

  Activation activation;

  ASSERT_OK(
      BindProtoToActivation(test_all_types, value_factory.get(), activation));

  EXPECT_THAT(activation.FindVariable(value_factory.get(), "map_int64_message"),
              IsOkAndHolds(Optional(IsMapValueOfSize(2))));
}

INSTANTIATE_TEST_SUITE_P(Runner, BindProtoToActivationTest,
                         ::testing::Values(MemoryManagerOption::kGlobal,
                                           MemoryManagerOption::kArena));

}  // namespace
}  // namespace cel::extensions
