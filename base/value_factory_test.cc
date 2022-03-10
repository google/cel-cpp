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

#include "base/value_factory.h"

#include "absl/status/status.h"
#include "base/memory_manager.h"
#include "internal/testing.h"

namespace cel {
namespace {

using cel::internal::StatusIs;

class TestValueFactory final : public ValueFactory {
 public:
  TestValueFactory() : ValueFactory(MemoryManager::Global()) {}
};

TEST(ValueFactory, CreateErrorValueReplacesOk) {
  TestValueFactory value_factory;
  EXPECT_THAT(value_factory.CreateErrorValue(absl::OkStatus())->value(),
              StatusIs(absl::StatusCode::kUnknown));
}

}  // namespace
}  // namespace cel
