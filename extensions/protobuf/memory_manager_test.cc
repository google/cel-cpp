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

#include "extensions/protobuf/memory_manager.h"

#include "google/protobuf/struct.pb.h"
#include "google/protobuf/arena.h"
#include "internal/testing.h"

namespace cel::extensions {
namespace {

struct NotArenaCompatible final {
  ~NotArenaCompatible() { Delete(); }

  MOCK_METHOD(void, Delete, (), ());
};

TEST(ProtoMemoryManager, ArenaConstructable) {
  google::protobuf::Arena arena;
  ProtoMemoryManager memory_manager(&arena);
  EXPECT_TRUE(
      google::protobuf::Arena::is_arena_constructable<google::protobuf::Value>::value);
  auto* object = NewInProtoArena<google::protobuf::Value>(memory_manager);
  EXPECT_NE(object, nullptr);
}

TEST(ProtoMemoryManager, NotArenaConstructable) {
  google::protobuf::Arena arena;
  ProtoMemoryManager memory_manager(&arena);
  EXPECT_FALSE(
      google::protobuf::Arena::is_arena_constructable<NotArenaCompatible>::value);
  auto* object = NewInProtoArena<NotArenaCompatible>(memory_manager);
  EXPECT_NE(object, nullptr);
  EXPECT_CALL(*object, Delete());
}

TEST(ProtoMemoryManagerNoArena, ArenaConstructable) {
  ProtoMemoryManager memory_manager(nullptr);
  EXPECT_TRUE(
      google::protobuf::Arena::is_arena_constructable<google::protobuf::Value>::value);
  auto* object = NewInProtoArena<google::protobuf::Value>(memory_manager);
  EXPECT_NE(object, nullptr);
  delete object;
}

TEST(ProtoMemoryManagerNoArena, NotArenaConstructable) {
  ProtoMemoryManager memory_manager(nullptr);
  EXPECT_FALSE(
      google::protobuf::Arena::is_arena_constructable<NotArenaCompatible>::value);
  auto* object = NewInProtoArena<NotArenaCompatible>(memory_manager);
  EXPECT_NE(object, nullptr);
  EXPECT_CALL(*object, Delete());
  delete object;
}

struct TriviallyDestructible final {};

struct NotTriviallyDestuctible final {
  ~NotTriviallyDestuctible() { Delete(); }

  MOCK_METHOD(void, Delete, (), ());
};

TEST(ProtoMemoryManager, TriviallyDestructible) {
  google::protobuf::Arena arena;
  ProtoMemoryManager memory_manager(&arena);
  EXPECT_TRUE(std::is_trivially_destructible_v<TriviallyDestructible>);
  auto managed = memory_manager.New<TriviallyDestructible>();
}

TEST(ProtoMemoryManager, NotTriviallyDestuctible) {
  google::protobuf::Arena arena;
  ProtoMemoryManager memory_manager(&arena);
  EXPECT_FALSE(std::is_trivially_destructible_v<NotTriviallyDestuctible>);
  auto managed = memory_manager.New<NotTriviallyDestuctible>();
  EXPECT_CALL(*managed, Delete());
}

TEST(ProtoMemoryManagerNoArena, TriviallyDestructible) {
  ProtoMemoryManager memory_manager(nullptr);
  EXPECT_TRUE(std::is_trivially_destructible_v<TriviallyDestructible>);
  auto managed = memory_manager.New<TriviallyDestructible>();
}

TEST(ProtoMemoryManagerNoArena, NotTriviallyDestuctible) {
  ProtoMemoryManager memory_manager(nullptr);
  EXPECT_FALSE(std::is_trivially_destructible_v<NotTriviallyDestuctible>);
  auto managed = memory_manager.New<NotTriviallyDestuctible>();
  EXPECT_CALL(*managed, Delete());
}

}  // namespace
}  // namespace cel::extensions
