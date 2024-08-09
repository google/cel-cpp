// Copyright 2024 Google LLC
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

#include "checker/standard_library.h"

#include "absl/status/status.h"
#include "checker/type_checker_builder.h"
#include "internal/testing.h"

namespace cel {
namespace {

using cel::internal::StatusIs;

TEST(StandardLibraryTest, StandardLibraryAddsDecls) {
  TypeCheckerBuilder builder;
  EXPECT_OK(builder.AddLibrary(StandardLibrary()));
  EXPECT_OK(std::move(builder).Build());
}

TEST(StandardLibraryTest, StandardLibraryErrorsIfAddedTwice) {
  TypeCheckerBuilder builder;
  EXPECT_OK(builder.AddLibrary(StandardLibrary()));
  EXPECT_THAT(builder.AddLibrary(StandardLibrary()),
              StatusIs(absl::StatusCode::kAlreadyExists));
}

}  // namespace
}  // namespace cel
