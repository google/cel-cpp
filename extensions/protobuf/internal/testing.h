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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_TESTING_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_TESTING_H_

#include <utility>

#include "absl/types/optional.h"
#include "base/internal/memory_manager_testing.h"
#include "base/memory_manager.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/testing.h"
#include "google/protobuf/arena.h"

namespace cel::extensions {

template <typename... Types>
class ProtoTest
    : public testing::TestWithParam<
          std::tuple<cel::base_internal::MemoryManagerTestMode, Types...>> {
  using Base = testing::TestWithParam<
      std::tuple<cel::base_internal::MemoryManagerTestMode, Types...>>;

 protected:
  void SetUp() override {
    if (std::get<0>(Base::GetParam()) ==
        cel::base_internal::MemoryManagerTestMode::kArena) {
      arena_.emplace();
      proto_memory_manager_.emplace(&arena_.value());
      memory_manager_ = &proto_memory_manager_.value();
    } else {
      memory_manager_ = &MemoryManager::Global();
    }
  }

  void TearDown() override {
    memory_manager_ = nullptr;
    if (std::get<0>(Base::GetParam()) ==
        cel::base_internal::MemoryManagerTestMode::kArena) {
      proto_memory_manager_.reset();
      arena_.reset();
    }
  }

  MemoryManager& memory_manager() const { return *memory_manager_; }

  const auto& test_case() const { return std::get<1>(Base::GetParam()); }

 private:
  absl::optional<google::protobuf::Arena> arena_;
  absl::optional<ProtoMemoryManager> proto_memory_manager_;
  MemoryManager* memory_manager_;
};

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_TESTING_H_