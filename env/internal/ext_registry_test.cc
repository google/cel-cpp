// Copyright 2026 Google LLC
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

#include "env/internal/ext_registry.h"

#include <string>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "checker/type_checker_builder.h"
#include "compiler/compiler.h"
#include "internal/testing.h"
#include "parser/parser_interface.h"

namespace cel::env_internal {
namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::Field;
using ::testing::HasSubstr;

TEST(ExtensionRegistryTest, GetCompilerLibrary) {
  ExtensionRegistry registry;
  registry.RegisterCompilerLibrary("foo1", "f", 1, []() {
    return CompilerLibrary("foo1_1", nullptr, nullptr);
  });
  registry.RegisterCompilerLibrary("foo1", "f", 2, []() {
    return CompilerLibrary("foo1_2", nullptr, nullptr);
  });
  registry.RegisterCompilerLibrary("foo2", "", 1, []() {
    return CompilerLibrary("foo2_1", nullptr, nullptr);
  });

  EXPECT_THAT(registry.GetCompilerLibrary("foo1", 1),
              IsOkAndHolds(Field(&CompilerLibrary::id, "foo1_1")));
  EXPECT_THAT(registry.GetCompilerLibrary("f", 1),
              IsOkAndHolds(Field(&CompilerLibrary::id, "foo1_1")));
  EXPECT_THAT(registry.GetCompilerLibrary("foo1", 2),
              IsOkAndHolds(Field(&CompilerLibrary::id, "foo1_2")));
  EXPECT_THAT(registry.GetCompilerLibrary("foo1", ExtensionRegistry::kLatest),
              IsOkAndHolds(Field(&CompilerLibrary::id, "foo1_2")));
  EXPECT_THAT(registry.GetCompilerLibrary("f", ExtensionRegistry::kLatest),
              IsOkAndHolds(Field(&CompilerLibrary::id, "foo1_2")));
  EXPECT_THAT(registry.GetCompilerLibrary("foo2", 1),
              IsOkAndHolds(Field(&CompilerLibrary::id, "foo2_1")));
  EXPECT_THAT(registry.GetCompilerLibrary("foo2", ExtensionRegistry::kLatest),
              IsOkAndHolds(Field(&CompilerLibrary::id, "foo2_1")));

  EXPECT_THAT(registry.GetCompilerLibrary("foo1", 3),
              StatusIs(absl::StatusCode::kNotFound,
                       HasSubstr("CompilerLibrary not registered: foo1#3")));
  EXPECT_THAT(registry.GetCompilerLibrary("foo3", 1),
              StatusIs(absl::StatusCode::kNotFound,
                       HasSubstr("CompilerLibrary not registered: foo3")));
  EXPECT_THAT(registry.GetCompilerLibrary("foo3", ExtensionRegistry::kLatest),
              StatusIs(absl::StatusCode::kNotFound,
                       HasSubstr("CompilerLibrary not registered: foo3")));
}

}  // namespace
}  // namespace cel::env_internal
