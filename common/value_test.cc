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
#include <utility>

#include "common/native_type.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::_;

template <typename T>
void IS_INITIALIZED(T&) {}

TEST(Value, KindDebugDeath) {
  Value moved_from_value = BoolValue(true);
  Value value = std::move(moved_from_value);
  IS_INITIALIZED(moved_from_value);
  static_cast<void>(value);
  EXPECT_DEBUG_DEATH(static_cast<void>(moved_from_value.kind()), _);
}

TEST(Value, GetTypeName) {
  Value moved_from_value = BoolValue(true);
  Value value = std::move(moved_from_value);
  IS_INITIALIZED(moved_from_value);
  static_cast<void>(value);
  EXPECT_DEBUG_DEATH(static_cast<void>(moved_from_value.GetTypeName()), _);
}

TEST(Value, DebugStringDebugDeath) {
  Value moved_from_value = BoolValue(true);
  Value value = std::move(moved_from_value);
  IS_INITIALIZED(moved_from_value);
  static_cast<void>(value);
  std::ostringstream out;
  EXPECT_DEBUG_DEATH(static_cast<void>(out << moved_from_value), _);
}

TEST(Value, NativeValueIdDebugDeath) {
  Value moved_from_value = BoolValue(true);
  Value value = std::move(moved_from_value);
  IS_INITIALIZED(moved_from_value);
  static_cast<void>(value);
  EXPECT_DEBUG_DEATH(static_cast<void>(NativeTypeId::Of(moved_from_value)), _);
}

TEST(ValueView, KindDebugDeath) {
  Value moved_from_value = BoolValue(true);
  Value value = std::move(moved_from_value);
  IS_INITIALIZED(moved_from_value);
  static_cast<void>(value);
  EXPECT_DEBUG_DEATH(static_cast<void>(ValueView(moved_from_value).kind()), _);
}

TEST(ValueView, GetTypeName) {
  Value moved_from_value = BoolValue(true);
  Value value = std::move(moved_from_value);
  IS_INITIALIZED(moved_from_value);
  static_cast<void>(value);
  EXPECT_DEBUG_DEATH(
      static_cast<void>(ValueView(moved_from_value).GetTypeName()), _);
}

TEST(ValueView, DebugStringDebugDeath) {
  Value moved_from_value = BoolValue(true);
  Value value = std::move(moved_from_value);
  IS_INITIALIZED(moved_from_value);
  static_cast<void>(value);
  std::ostringstream out;
  EXPECT_DEBUG_DEATH(static_cast<void>(out << ValueView(moved_from_value)), _);
}

TEST(ValueView, NativeValueIdDebugDeath) {
  Value moved_from_value = BoolValue(true);
  Value value = std::move(moved_from_value);
  IS_INITIALIZED(moved_from_value);
  static_cast<void>(value);
  EXPECT_DEBUG_DEATH(
      static_cast<void>(NativeTypeId::Of(ValueView(moved_from_value))), _);
}

}  // namespace
}  // namespace cel
