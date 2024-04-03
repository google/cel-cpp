// Copyright 2024 Google LLC
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

#include "runtime/optional_types.h"

#include <vector>

#include "absl/status/status.h"
#include "base/function_descriptor.h"
#include "common/kind.h"
#include "internal/testing.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"

namespace cel::extensions {
namespace {

using testing::ElementsAre;
using cel::internal::IsOk;
using cel::internal::StatusIs;

MATCHER_P(MatchesOptionalReceiver1, name, "") {
  const FunctionDescriptor& descriptor = arg.descriptor;

  std::vector<Kind> types{Kind::kOpaque};
  return descriptor.name() == name && descriptor.receiver_style() == true &&
         descriptor.types() == types;
}

MATCHER_P2(MatchesOptionalReceiver2, name, kind, "") {
  const FunctionDescriptor& descriptor = arg.descriptor;

  std::vector<Kind> types{Kind::kOpaque, kind};
  return descriptor.name() == name && descriptor.receiver_style() == true &&
         descriptor.types() == types;
}

MATCHER_P2(MatchesOptionalSelect, kind1, kind2, "") {
  const FunctionDescriptor& descriptor = arg.descriptor;

  std::vector<Kind> types{kind1, kind2};
  return descriptor.name() == "_?._" && descriptor.receiver_style() == false &&
         descriptor.types() == types;
}

MATCHER_P2(MatchesOptionalIndex, kind1, kind2, "") {
  const FunctionDescriptor& descriptor = arg.descriptor;

  std::vector<Kind> types{kind1, kind2};
  return descriptor.name() == "_[?_]" && descriptor.receiver_style() == false &&
         descriptor.types() == types;
}

TEST(EnableOptionalTypes, HeterogeneousEqualityRequired) {
  ASSERT_OK_AND_ASSIGN(auto builder,
                       CreateStandardRuntimeBuilder(RuntimeOptions{
                           .enable_qualified_type_identifiers = true,
                           .enable_heterogeneous_equality = false}));
  EXPECT_THAT(EnableOptionalTypes(builder),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(EnableOptionalTypes, QualifiedTypeIdentifiersRequired) {
  ASSERT_OK_AND_ASSIGN(auto builder,
                       CreateStandardRuntimeBuilder(RuntimeOptions{
                           .enable_qualified_type_identifiers = false,
                           .enable_heterogeneous_equality = true}));
  EXPECT_THAT(EnableOptionalTypes(builder),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(EnableOptionalTypes, PreconditionsSatisfied) {
  ASSERT_OK_AND_ASSIGN(auto builder,
                       CreateStandardRuntimeBuilder(RuntimeOptions{
                           .enable_qualified_type_identifiers = true,
                           .enable_heterogeneous_equality = true}));
  EXPECT_THAT(EnableOptionalTypes(builder), IsOk());
}

TEST(EnableOptionalTypes, Functions) {
  ASSERT_OK_AND_ASSIGN(auto builder,
                       CreateStandardRuntimeBuilder(RuntimeOptions{
                           .enable_qualified_type_identifiers = true,
                           .enable_heterogeneous_equality = true}));
  ASSERT_THAT(EnableOptionalTypes(builder), IsOk());

  EXPECT_THAT(builder.function_registry().FindStaticOverloads("hasValue", true,
                                                              {Kind::kOpaque}),
              ElementsAre(MatchesOptionalReceiver1("hasValue")));

  EXPECT_THAT(builder.function_registry().FindStaticOverloads("value", true,
                                                              {Kind::kOpaque}),
              ElementsAre(MatchesOptionalReceiver1("value")));

  EXPECT_THAT(builder.function_registry().FindStaticOverloads(
                  "or", true, {Kind::kOpaque, Kind::kOpaque}),
              ElementsAre(MatchesOptionalReceiver2("or", Kind::kOpaque)));

  EXPECT_THAT(builder.function_registry().FindStaticOverloads(
                  "orValue", true, {Kind::kOpaque, Kind::kAny}),
              ElementsAre(MatchesOptionalReceiver2("orValue", Kind::kAny)));

  EXPECT_THAT(builder.function_registry().FindStaticOverloads(
                  "_?._", false, {Kind::kStruct, Kind::kString}),
              ElementsAre(MatchesOptionalSelect(Kind::kStruct, Kind::kString)));
  EXPECT_THAT(builder.function_registry().FindStaticOverloads(
                  "_?._", false, {Kind::kMap, Kind::kString}),
              ElementsAre(MatchesOptionalSelect(Kind::kMap, Kind::kString)));
  EXPECT_THAT(builder.function_registry().FindStaticOverloads(
                  "_?._", false, {Kind::kOpaque, Kind::kString}),
              ElementsAre(MatchesOptionalSelect(Kind::kOpaque, Kind::kString)));

  EXPECT_THAT(builder.function_registry().FindStaticOverloads(
                  "_[?_]", false, {Kind::kMap, Kind::kAny}),
              ElementsAre(MatchesOptionalIndex(Kind::kMap, Kind::kAny)));
  EXPECT_THAT(builder.function_registry().FindStaticOverloads(
                  "_[?_]", false, {Kind::kList, Kind::kInt}),
              ElementsAre(MatchesOptionalIndex(Kind::kList, Kind::kInt)));
  EXPECT_THAT(builder.function_registry().FindStaticOverloads(
                  "_[?_]", false, {Kind::kOpaque, Kind::kAny}),
              ElementsAre(MatchesOptionalIndex(Kind::kOpaque, Kind::kAny)));
}

}  // namespace
}  // namespace cel::extensions
