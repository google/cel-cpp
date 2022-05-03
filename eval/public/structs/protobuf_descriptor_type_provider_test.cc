// Copyright 2022 Google LLC
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

#include "eval/public/structs/protobuf_descriptor_type_provider.h"

#include "eval/public/cel_value.h"
#include "eval/public/testing/matchers.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {
namespace {

TEST(ProtobufDescriptorProvider, Basic) {
  ProtobufDescriptorProvider provider(
      google::protobuf::DescriptorPool::generated_pool(),
      google::protobuf::MessageFactory::generated_factory());
  google::protobuf::Arena arena;
  cel::extensions::ProtoMemoryManager manager(&arena);
  auto type_adapter = provider.ProvideLegacyType("google.protobuf.Int64Value");

  ASSERT_TRUE(type_adapter.has_value());
  ASSERT_TRUE(type_adapter->mutation_apis() != nullptr);

  ASSERT_TRUE(type_adapter->mutation_apis()->DefinesField("value"));
  ASSERT_OK_AND_ASSIGN(CelValue::MessageWrapper::Builder value,
                       type_adapter->mutation_apis()->NewInstance(manager));

  ASSERT_OK(type_adapter->mutation_apis()->SetField(
      "value", CelValue::CreateInt64(10), manager, value));

  ASSERT_OK_AND_ASSIGN(
      CelValue adapted,
      type_adapter->mutation_apis()->AdaptFromWellKnownType(manager, value));

  EXPECT_THAT(adapted, test::IsCelInt64(10));
}

// This is an implementation detail, but testing for coverage.
TEST(ProtobufDescriptorProvider, MemoizesAdapters) {
  ProtobufDescriptorProvider provider(
      google::protobuf::DescriptorPool::generated_pool(),
      google::protobuf::MessageFactory::generated_factory());
  google::protobuf::Arena arena;
  cel::extensions::ProtoMemoryManager manager(&arena);
  auto type_adapter = provider.ProvideLegacyType("google.protobuf.Int64Value");

  ASSERT_TRUE(type_adapter.has_value());
  ASSERT_TRUE(type_adapter->mutation_apis() != nullptr);

  auto type_adapter2 = provider.ProvideLegacyType("google.protobuf.Int64Value");
  ASSERT_TRUE(type_adapter2.has_value());

  EXPECT_EQ(type_adapter->mutation_apis(), type_adapter2->mutation_apis());
  EXPECT_EQ(type_adapter->access_apis(), type_adapter2->access_apis());
}

TEST(ProtobufDescriptorProvider, NotFound) {
  ProtobufDescriptorProvider provider(
      google::protobuf::DescriptorPool::generated_pool(),
      google::protobuf::MessageFactory::generated_factory());
  google::protobuf::Arena arena;
  cel::extensions::ProtoMemoryManager manager(&arena);
  auto type_adapter = provider.ProvideLegacyType("UnknownType");

  ASSERT_FALSE(type_adapter.has_value());
}

}  // namespace
}  // namespace google::api::expr::runtime
