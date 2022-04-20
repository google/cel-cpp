// Copyright 2022 Google LLC
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

#include "eval/public/portable_cel_expr_builder_factory.h"

#include <utility>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/dynamic_message.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/structs/cel_proto_descriptor_pool_builder.h"
#include "eval/public/structs/protobuf_descriptor_type_provider.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {
namespace {

TEST(PortableCelExprBuilderFactoryTest, CreateNullOnMissingTypeProvider) {
  std::unique_ptr<CelExpressionBuilder> builder =
      CreatePortableExprBuilder(nullptr);
  ASSERT_EQ(builder, nullptr);
}

TEST(PortableCelExprBuilderFactoryTest, CreateSuccess) {
  google::protobuf::DescriptorPool descriptor_pool;
  google::protobuf::Arena arena;

  // Setup descriptor pool and builder
  ASSERT_OK(AddStandardMessageTypesToDescriptorPool(descriptor_pool));
  google::protobuf::DynamicMessageFactory message_factory(&descriptor_pool);
  auto type_provider = std::make_unique<ProtobufDescriptorProvider>(
      &descriptor_pool, &message_factory);
  std::unique_ptr<CelExpressionBuilder> builder =
      CreatePortableExprBuilder(std::move(type_provider));
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));
}

}  // namespace
}  // namespace google::api::expr::runtime
