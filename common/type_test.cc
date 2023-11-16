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

#include "common/type.h"

#include <sstream>
#include <utility>

#include "absl/hash/hash.h"
#include "common/native_type.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::_;

template <typename T>
void IS_INITIALIZED(T&) {}

TEST(Type, KindDebugDeath) {
  Type moved_from_type = AnyType();
  Type type = std::move(moved_from_type);
  IS_INITIALIZED(moved_from_type);
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(static_cast<void>(moved_from_type.kind()), _);
}

TEST(Type, NameDebugDeath) {
  Type moved_from_type = AnyType();
  Type type = std::move(moved_from_type);
  IS_INITIALIZED(moved_from_type);
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(static_cast<void>(moved_from_type.name()), _);
}

TEST(Type, DebugStringDebugDeath) {
  Type moved_from_type = AnyType();
  Type type = std::move(moved_from_type);
  IS_INITIALIZED(moved_from_type);
  static_cast<void>(type);
  std::ostringstream out;
  EXPECT_DEBUG_DEATH(static_cast<void>(out << moved_from_type), _);
}

TEST(Type, HashDebugDeath) {
  Type moved_from_type = AnyType();
  Type type = std::move(moved_from_type);
  IS_INITIALIZED(moved_from_type);
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(static_cast<void>(absl::HashOf(moved_from_type)), _);
}

TEST(Type, EqualDebugDeath) {
  Type moved_from_type = AnyType();
  Type type = std::move(moved_from_type);
  IS_INITIALIZED(moved_from_type);
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(static_cast<void>(type == moved_from_type), _);
  EXPECT_DEBUG_DEATH(static_cast<void>(moved_from_type == type), _);
}

TEST(Type, NativeTypeIdDebugDeath) {
  Type moved_from_type = AnyType();
  Type type = std::move(moved_from_type);
  IS_INITIALIZED(moved_from_type);
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(static_cast<void>(NativeTypeId::Of(moved_from_type)), _);
}

TEST(TypeView, KindDebugDeath) {
  Type moved_from_type = AnyType();
  Type type = std::move(moved_from_type);
  IS_INITIALIZED(moved_from_type);
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(static_cast<void>(TypeView(moved_from_type).kind()), _);
}

TEST(TypeView, NameDebugDeath) {
  Type moved_from_type = AnyType();
  Type type = std::move(moved_from_type);
  IS_INITIALIZED(moved_from_type);
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(static_cast<void>(TypeView(moved_from_type).name()), _);
}

TEST(TypeView, DebugStringDebugDeath) {
  Type moved_from_type = AnyType();
  Type type = std::move(moved_from_type);
  IS_INITIALIZED(moved_from_type);
  static_cast<void>(type);
  std::ostringstream out;
  EXPECT_DEBUG_DEATH(static_cast<void>(out << TypeView(moved_from_type)), _);
}

TEST(TypeView, HashDebugDeath) {
  Type moved_from_type = AnyType();
  Type type = std::move(moved_from_type);
  IS_INITIALIZED(moved_from_type);
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(static_cast<void>(absl::HashOf(TypeView(moved_from_type))),
                     _);
}

TEST(TypeView, EqualDebugDeath) {
  Type moved_from_type = AnyType();
  Type type = std::move(moved_from_type);
  IS_INITIALIZED(moved_from_type);
  EXPECT_DEBUG_DEATH(
      static_cast<void>(TypeView(type) == TypeView(moved_from_type)), _);
  EXPECT_DEBUG_DEATH(
      static_cast<void>(TypeView(moved_from_type) == TypeView(type)), _);
}

TEST(TypeView, NativeTypeIdDebugDeath) {
  Type moved_from_type = AnyType();
  Type type = std::move(moved_from_type);
  IS_INITIALIZED(moved_from_type);
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(
      static_cast<void>(NativeTypeId::Of(TypeView(moved_from_type))), _);
}

}  // namespace
}  // namespace cel
