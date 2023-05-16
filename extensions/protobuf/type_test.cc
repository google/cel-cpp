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

#include "extensions/protobuf/type.h"

#include "google/protobuf/api.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "absl/status/status.h"
#include "base/internal/memory_manager_testing.h"
#include "base/testing/type_matchers.h"
#include "base/type_factory.h"
#include "extensions/protobuf/internal/testing.h"
#include "extensions/protobuf/type_provider.h"
#include "internal/testing.h"

namespace cel::extensions {
namespace {

using ::cel_testing::TypeIs;
using testing::status::CanonicalStatusIs;
using cel::internal::IsOkAndHolds;

using ProtoTypeTest = ProtoTest<>;

TEST_P(ProtoTypeTest, StaticWrapperTypes) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  EXPECT_THAT(ProtoType::Resolve<google::protobuf::BoolValue>(type_manager),
              IsOkAndHolds(TypeIs<BoolWrapperType>()));
  EXPECT_THAT(ProtoType::Resolve<google::protobuf::BytesValue>(type_manager),
              IsOkAndHolds(TypeIs<BytesWrapperType>()));
  EXPECT_THAT(ProtoType::Resolve<google::protobuf::FloatValue>(type_manager),
              IsOkAndHolds(TypeIs<DoubleWrapperType>()));
  EXPECT_THAT(ProtoType::Resolve<google::protobuf::DoubleValue>(type_manager),
              IsOkAndHolds(TypeIs<DoubleWrapperType>()));
  EXPECT_THAT(ProtoType::Resolve<google::protobuf::Int32Value>(type_manager),
              IsOkAndHolds(TypeIs<IntWrapperType>()));
  EXPECT_THAT(ProtoType::Resolve<google::protobuf::Int64Value>(type_manager),
              IsOkAndHolds(TypeIs<IntWrapperType>()));
  EXPECT_THAT(ProtoType::Resolve<google::protobuf::StringValue>(type_manager),
              IsOkAndHolds(TypeIs<StringWrapperType>()));
  EXPECT_THAT(ProtoType::Resolve<google::protobuf::UInt32Value>(type_manager),
              IsOkAndHolds(TypeIs<UintWrapperType>()));
  EXPECT_THAT(ProtoType::Resolve<google::protobuf::UInt64Value>(type_manager),
              IsOkAndHolds(TypeIs<UintWrapperType>()));
}

TEST_P(ProtoTypeTest, DynamicWrapperTypes) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  EXPECT_THAT(ProtoType::Resolve(type_manager,
                                 *google::protobuf::BoolValue::descriptor()),
              IsOkAndHolds(TypeIs<BoolWrapperType>()));
  EXPECT_THAT(ProtoType::Resolve(type_manager,
                                 *google::protobuf::BytesValue::descriptor()),
              IsOkAndHolds(TypeIs<BytesWrapperType>()));
  EXPECT_THAT(ProtoType::Resolve(type_manager,
                                 *google::protobuf::FloatValue::descriptor()),
              IsOkAndHolds(TypeIs<DoubleWrapperType>()));
  EXPECT_THAT(ProtoType::Resolve(type_manager,
                                 *google::protobuf::DoubleValue::descriptor()),
              IsOkAndHolds(TypeIs<DoubleWrapperType>()));
  EXPECT_THAT(ProtoType::Resolve(type_manager,
                                 *google::protobuf::Int32Value::descriptor()),
              IsOkAndHolds(TypeIs<IntWrapperType>()));
  EXPECT_THAT(ProtoType::Resolve(type_manager,
                                 *google::protobuf::Int64Value::descriptor()),
              IsOkAndHolds(TypeIs<IntWrapperType>()));
  EXPECT_THAT(ProtoType::Resolve(type_manager,
                                 *google::protobuf::StringValue::descriptor()),
              IsOkAndHolds(TypeIs<StringWrapperType>()));
  EXPECT_THAT(ProtoType::Resolve(type_manager,
                                 *google::protobuf::UInt32Value::descriptor()),
              IsOkAndHolds(TypeIs<UintWrapperType>()));
  EXPECT_THAT(ProtoType::Resolve(type_manager,
                                 *google::protobuf::UInt64Value::descriptor()),
              IsOkAndHolds(TypeIs<UintWrapperType>()));
}

TEST_P(ProtoTypeTest, ResolveNotFound) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  EXPECT_THAT(
      ProtoType::Resolve(type_manager, *google::protobuf::Api::descriptor()),
      CanonicalStatusIs(absl::StatusCode::kNotFound));
}

INSTANTIATE_TEST_SUITE_P(ProtoTypeTest, ProtoTypeTest,
                         cel::base_internal::MemoryManagerTestModeAll(),
                         cel::base_internal::MemoryManagerTestModeTupleName);

}  // namespace
}  // namespace cel::extensions
