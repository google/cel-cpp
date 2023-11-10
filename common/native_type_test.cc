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

#include "common/native_type.h"

#include <sstream>

#include "absl/hash/hash_testing.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::IsEmpty;
using testing::Not;

struct Type1 {};

struct Type2 {};

struct Type3 {};

TEST(NativeTypeId, ImplementsAbslHashCorrectly) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      {NativeTypeId(), NativeTypeId::For<Type1>(), NativeTypeId::For<Type2>(),
       NativeTypeId::For<Type3>()}));
}

TEST(NativeTypeId, DebugString) {
  std::ostringstream out;
  out << NativeTypeId();
  EXPECT_THAT(out.str(), IsEmpty());
  out << NativeTypeId::For<Type1>();
  EXPECT_THAT(out.str(), Not(IsEmpty()));
}

namespace native_type_test {

struct TestType {};

NativeTypeId CelNativeTypeIdOf(const TestType& type) {
  return NativeTypeId::For<TestType>();
}

}  // namespace native_type_test

TEST(NativeTypeId, Of) {
  EXPECT_EQ(NativeTypeId::Of(native_type_test::TestType()),
            NativeTypeId::For<native_type_test::TestType>());
}

}  // namespace
}  // namespace cel
