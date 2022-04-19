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

#include "eval/public/structs/legacy_type_adapter.h"

#include "google/protobuf/arena.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/trivial_legacy_type_info.h"
#include "eval/public/testing/matchers.h"
#include "eval/testutil/test_message.pb.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {
namespace {
using testing::EqualsProto;

class TestMutationApiImpl : public LegacyTypeMutationApis {
 public:
  TestMutationApiImpl() {}
  bool DefinesField(absl::string_view field_name) const override {
    return false;
  }

  absl::StatusOr<CelValue::MessageWrapper> NewInstance(
      cel::MemoryManager& memory_manager) const override {
    return absl::UnimplementedError("Not implemented");
  }

  absl::Status SetField(absl::string_view field_name, const CelValue& value,
                        cel::MemoryManager& memory_manager,
                        CelValue::MessageWrapper& instance) const override {
    return absl::UnimplementedError("Not implemented");
  }
};

class TestAccessApiImpl : public LegacyTypeAccessApis {
 public:
  TestAccessApiImpl() {}
  absl::StatusOr<bool> HasField(
      absl::string_view field_name,
      const CelValue::MessageWrapper& value) const override {
    return absl::UnimplementedError("Not implemented");
  }

  absl::StatusOr<CelValue> GetField(
      absl::string_view field_name, const CelValue::MessageWrapper& instance,
      ProtoWrapperTypeOptions unboxing_option,
      cel::MemoryManager& memory_manager) const override {
    return absl::UnimplementedError("Not implemented");
  }
};

TEST(LegacyTypeAdapterMutationApis, DefaultNoopAdapt) {
  TestMessage message;
  internal::MessageWrapper wrapper(&message, TrivialTypeInfo::GetInstance());
  google::protobuf::Arena arena;
  cel::extensions::ProtoMemoryManager manager(&arena);

  TestMutationApiImpl impl;

  ASSERT_OK_AND_ASSIGN(CelValue v,
                       impl.AdaptFromWellKnownType(manager, wrapper));

  EXPECT_THAT(v,
              test::IsCelMessage(EqualsProto(TestMessage::default_instance())));
}

TEST(LegacyTypeAdapterAccessApis, DefaultAlwaysInequal) {
  TestMessage message;
  internal::MessageWrapper wrapper(&message, nullptr);
  internal::MessageWrapper wrapper2(&message, nullptr);

  google::protobuf::Arena arena;
  cel::extensions::ProtoMemoryManager manager(&arena);

  TestAccessApiImpl impl;

  EXPECT_FALSE(impl.IsEqual(wrapper, wrapper2));
}

}  // namespace
}  // namespace google::api::expr::runtime
