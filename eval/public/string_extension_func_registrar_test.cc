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

#include "eval/public/string_extension_func_registrar.h"

#include <cstdint>
#include <string>
#include <vector>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_value.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {
namespace {
using google::protobuf::Arena;

class StringExtensionTest : public ::testing::Test {
 protected:
  StringExtensionTest() {}
  void SetUp() override {
    ASSERT_OK(RegisterBuiltinFunctions(&registry_));
    ASSERT_OK(RegisterStringExtensionFunctions(&registry_));
  }

  void PerformSplitStringTest(Arena* arena, std::string* value,
                              std::string* delimiter, CelValue* result) {
    auto function = registry_.FindOverloads(
        "split", true, {CelValue::Type::kString, CelValue::Type::kString});
    ASSERT_EQ(function.size(), 1);
    auto func = function[0];
    std::vector<CelValue> args = {CelValue::CreateString(value),
                                  CelValue::CreateString(delimiter)};
    absl::Span<CelValue> arg_span(&args[0], args.size());
    auto status = func->Evaluate(arg_span, result, arena);
    ASSERT_OK(status);
  }

  void PerformSplitStringWithLimitTest(Arena* arena, std::string* value,
                                       std::string* delimiter, int64_t limit,
                                       CelValue* result) {
    auto function = registry_.FindOverloads(
        "split", true,
        {CelValue::Type::kString, CelValue::Type::kString,
         CelValue::Type::kInt64});
    ASSERT_EQ(function.size(), 1);
    auto func = function[0];
    std::vector<CelValue> args = {CelValue::CreateString(value),
                                  CelValue::CreateString(delimiter),
                                  CelValue::CreateInt64(limit)};
    absl::Span<CelValue> arg_span(&args[0], args.size());
    auto status = func->Evaluate(arg_span, result, arena);
    ASSERT_OK(status);
  }
  // Function registry
  CelFunctionRegistry registry_;
  Arena arena_;
};

TEST_F(StringExtensionTest, TestStringSplit) {
  Arena arena;
  CelValue result;
  std::string value = "This!!Is!!Test";
  std::string delimiter = "!!";
  std::vector<std::string> expected = {"This", "Is", "Test"};

  EXPECT_NO_FATAL_FAILURE(
      PerformSplitStringTest(&arena, &value, &delimiter, &result));
  ASSERT_EQ(result.type(), CelValue::Type::kList);
  EXPECT_EQ(result.ListOrDie()->size(), 3);
  for (int i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(result.ListOrDie()->Get(&arena, i).StringOrDie().value(),
              expected[i]);
  }
}

TEST_F(StringExtensionTest, TestStringSplitEmptyDelimiter) {
  Arena arena;
  CelValue result;
  std::string value = "TEST";
  std::string delimiter = "";
  std::vector<std::string> expected = {"T", "E", "S", "T"};

  EXPECT_NO_FATAL_FAILURE(
      PerformSplitStringTest(&arena, &value, &delimiter, &result));
  ASSERT_EQ(result.type(), CelValue::Type::kList);
  EXPECT_EQ(result.ListOrDie()->size(), 4);
  for (int i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(result.ListOrDie()->Get(&arena, i).StringOrDie().value(),
              expected[i]);
  }
}

TEST_F(StringExtensionTest, TestStringSplitWithLimitTwo) {
  Arena arena;
  CelValue result;
  int64_t limit = 2;
  std::string value = "This!!Is!!Test";
  std::string delimiter = "!!";
  std::vector<std::string> expected = {"This", "Is!!Test"};

  EXPECT_NO_FATAL_FAILURE(PerformSplitStringWithLimitTest(
      &arena, &value, &delimiter, limit, &result));
  ASSERT_EQ(result.type(), CelValue::Type::kList);
  EXPECT_EQ(result.ListOrDie()->size(), 2);
  for (int i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(result.ListOrDie()->Get(&arena, i).StringOrDie().value(),
              expected[i]);
  }
}

TEST_F(StringExtensionTest, TestStringSplitWithLimitOne) {
  Arena arena;
  CelValue result;
  int64_t limit = 1;
  std::string value = "This!!Is!!Test";
  std::string delimiter = "!!";
  EXPECT_NO_FATAL_FAILURE(PerformSplitStringWithLimitTest(
      &arena, &value, &delimiter, limit, &result));
  ASSERT_EQ(result.type(), CelValue::Type::kList);
  EXPECT_EQ(result.ListOrDie()->size(), 1);
  EXPECT_EQ(result.ListOrDie()->Get(&arena, 0).StringOrDie().value(), value);
}

TEST_F(StringExtensionTest, TestStringSplitWithLimitZero) {
  Arena arena;
  CelValue result;
  int64_t limit = 0;
  std::string value = "This!!Is!!Test";
  std::string delimiter = "!!";
  EXPECT_NO_FATAL_FAILURE(PerformSplitStringWithLimitTest(
      &arena, &value, &delimiter, limit, &result));
  ASSERT_EQ(result.type(), CelValue::Type::kList);
  EXPECT_EQ(result.ListOrDie()->size(), 0);
}

TEST_F(StringExtensionTest, TestStringSplitWithLimitNegative) {
  Arena arena;
  CelValue result;
  int64_t limit = -1;
  std::string value = "This!!Is!!Test";
  std::string delimiter = "!!";
  std::vector<std::string> expected = {"This", "Is", "Test"};
  EXPECT_NO_FATAL_FAILURE(PerformSplitStringWithLimitTest(
      &arena, &value, &delimiter, limit, &result));
  ASSERT_EQ(result.type(), CelValue::Type::kList);
  EXPECT_EQ(result.ListOrDie()->size(), 3);
  for (int i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(result.ListOrDie()->Get(&arena, i).StringOrDie().value(),
              expected[i]);
  }
}

TEST_F(StringExtensionTest, TestStringSplitWithLimitAsMaxPossibleSplits) {
  Arena arena;
  CelValue result;
  int64_t limit = 3;
  std::string value = "This!!Is!!Test";
  std::string delimiter = "!!";
  std::vector<std::string> expected = {"This", "Is", "Test"};

  EXPECT_NO_FATAL_FAILURE(PerformSplitStringWithLimitTest(
      &arena, &value, &delimiter, limit, &result));
  ASSERT_EQ(result.type(), CelValue::Type::kList);
  EXPECT_EQ(result.ListOrDie()->size(), 3);
  for (int i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(result.ListOrDie()->Get(&arena, i).StringOrDie().value(),
              expected[i]);
  }
}

TEST_F(StringExtensionTest,
       TestStringSplitWithLimitGreaterThanMaxPossibleSplits) {
  Arena arena;
  CelValue result;
  int64_t limit = 4;
  std::string value = "This!!Is!!Test";
  std::string delimiter = "!!";
  std::vector<std::string> expected = {"This", "Is", "Test"};

  EXPECT_NO_FATAL_FAILURE(PerformSplitStringWithLimitTest(
      &arena, &value, &delimiter, limit, &result));
  ASSERT_EQ(result.type(), CelValue::Type::kList);
  EXPECT_EQ(result.ListOrDie()->size(), 3);
  for (int i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(result.ListOrDie()->Get(&arena, i).StringOrDie().value(),
              expected[i]);
  }
}

}  // namespace
}  // namespace google::api::expr::runtime
