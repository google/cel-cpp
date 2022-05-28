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

TEST(ValueFactory, CreateErrorValueReplacesOk) {
  TypeFactory type_factory(MemoryManager::Global());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_THAT(value_factory.CreateErrorValue(absl::OkStatus())->value(),
              StatusIs(absl::StatusCode::kUnknown));
}

TEST(ValueFactory, CreateStringValueIllegalByteSequence) {
  TypeFactory type_factory(MemoryManager::Global());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_THAT(value_factory.CreateStringValue("\xff"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(value_factory.CreateStringValue(absl::Cord("\xff")),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

}  // namespace
}  // namespace cel
