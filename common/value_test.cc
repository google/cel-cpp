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

#include "common/value.h"

#include <sstream>

#include "common/native_type.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::_;

TEST(Value, KindDebugDeath) {
  Value value;
  static_cast<void>(value);
  EXPECT_DEBUG_DEATH(static_cast<void>(value.kind()), _);
}

TEST(Value, GetTypeName) {
  Value value;
  static_cast<void>(value);
  EXPECT_DEBUG_DEATH(static_cast<void>(value.GetTypeName()), _);
}

TEST(Value, DebugStringUinitializedValue) {
  Value value;
  static_cast<void>(value);
  std::ostringstream out;
  out << value;
  EXPECT_EQ(out.str(), "default ctor Value");
}

TEST(Value, NativeValueIdDebugDeath) {
  Value value;
  static_cast<void>(value);
  EXPECT_DEBUG_DEATH(static_cast<void>(NativeTypeId::Of(value)), _);
}

}  // namespace
}  // namespace cel
