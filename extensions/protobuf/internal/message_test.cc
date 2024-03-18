// Copyright 2024 Google LLC
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

#include "extensions/protobuf/internal/message.h"

#include "internal/testing.h"
#include "proto/test/v1/proto2/test_all_types.pb.h"

namespace cel::extensions::protobuf_internal {
namespace {

using ::google::api::expr::test::v1::proto2::TestAllTypes;
using testing::NotNull;
using cel::internal::IsOkAndHolds;

TEST(GetDescriptor, NotNull) {
  TestAllTypes message;
  EXPECT_THAT(GetDescriptor(message), IsOkAndHolds(NotNull()));
}

TEST(GetReflection, NotNull) {
  TestAllTypes message;
  EXPECT_THAT(GetReflection(message), IsOkAndHolds(NotNull()));
}

TEST(GetReflectionOrDie, DoesNotDie) {
  TestAllTypes message;
  EXPECT_THAT(GetReflectionOrDie(message), NotNull());
}

}  // namespace
}  // namespace cel::extensions::protobuf_internal
