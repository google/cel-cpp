//  Copyright 2022 Google LLC
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//       https://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#include "eval/public/portable_cel_function_adapter.h"

#include <functional>
#include <string>
#include <utility>

#include "internal/status_macros.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

namespace {

TEST(PortableCelFunctionAdapterTest, TestAdapterNoArg) {
  auto func = [](google::protobuf::Arena*) -> int64_t { return 100; };
  ASSERT_OK_AND_ASSIGN(auto cel_func, (PortableFunctionAdapter<int64_t>::Create(
                                          "const", false, func)));

  absl::Span<CelValue> args;
  CelValue result = CelValue::CreateNull();
  google::protobuf::Arena arena;
  ASSERT_OK(cel_func->Evaluate(args, &result, &arena));
  // Obvious failure, for educational purposes only.
  ASSERT_TRUE(result.IsInt64());
}

TEST(PortableCelFunctionAdapterTest, TestAdapterOneArg) {
  std::function<int64_t(google::protobuf::Arena*, int64_t)> func =
      [](google::protobuf::Arena* arena, int64_t i) -> int64_t { return i + 1; };
  ASSERT_OK_AND_ASSIGN(
      auto cel_func,
      (PortableFunctionAdapter<int64_t, int64_t>::Create("_++_", false, func)));

  std::vector<CelValue> args_vec;
  args_vec.push_back(CelValue::CreateInt64(99));

  CelValue result = CelValue::CreateNull();
  google::protobuf::Arena arena;

  absl::Span<CelValue> args(&args_vec[0], args_vec.size());
  ASSERT_OK(cel_func->Evaluate(args, &result, &arena));
  ASSERT_TRUE(result.IsInt64());
  EXPECT_EQ(result.Int64OrDie(), 100);
}

TEST(PortableCelFunctionAdapterTest, TestAdapterTwoArgs) {
  auto func = [](google::protobuf::Arena* arena, int64_t i, int64_t j) -> int64_t {
    return i + j;
  };
  ASSERT_OK_AND_ASSIGN(auto cel_func,
                       (PortableFunctionAdapter<int64_t, int64_t, int64_t>::Create(
                           "_++_", false, func)));

  std::vector<CelValue> args_vec;
  args_vec.push_back(CelValue::CreateInt64(20));
  args_vec.push_back(CelValue::CreateInt64(22));

  CelValue result = CelValue::CreateNull();
  google::protobuf::Arena arena;

  absl::Span<CelValue> args(&args_vec[0], args_vec.size());
  ASSERT_OK(cel_func->Evaluate(args, &result, &arena));
  ASSERT_TRUE(result.IsInt64());
  EXPECT_EQ(result.Int64OrDie(), 42);
}

using StringHolder = CelValue::StringHolder;

TEST(PortableCelFunctionAdapterTest, TestAdapterThreeArgs) {
  auto func = [](google::protobuf::Arena* arena, StringHolder s1, StringHolder s2,
                 StringHolder s3) -> StringHolder {
    std::string value = absl::StrCat(s1.value(), s2.value(), s3.value());

    return StringHolder(
        google::protobuf::Arena::Create<std::string>(arena, std::move(value)));
  };
  ASSERT_OK_AND_ASSIGN(
      auto cel_func,
      (PortableFunctionAdapter<StringHolder, StringHolder, StringHolder,
                               StringHolder>::Create("concat", false, func)));

  std::string test1 = "1";
  std::string test2 = "2";
  std::string test3 = "3";

  std::vector<CelValue> args_vec;
  args_vec.push_back(CelValue::CreateString(&test1));
  args_vec.push_back(CelValue::CreateString(&test2));
  args_vec.push_back(CelValue::CreateString(&test3));

  CelValue result = CelValue::CreateNull();
  google::protobuf::Arena arena;

  absl::Span<CelValue> args(&args_vec[0], args_vec.size());
  ASSERT_OK(cel_func->Evaluate(args, &result, &arena));
  ASSERT_TRUE(result.IsString());
  EXPECT_EQ(result.StringOrDie().value(), "123");
}

TEST(PortableCelFunctionAdapterTest, TestTypeDeductionForCelValueBasicTypes) {
  auto func = [](google::protobuf::Arena* arena, bool, int64_t, uint64_t, double,
                 CelValue::StringHolder, CelValue::BytesHolder,
                 CelValue::MessageWrapper, absl::Duration, absl::Time,
                 const CelList*, const CelMap*,
                 const CelError*) -> bool { return false; };
  ASSERT_OK_AND_ASSIGN(
      auto cel_func,
      (PortableFunctionAdapter<bool, bool, int64_t, uint64_t, double,
                               CelValue::StringHolder, CelValue::BytesHolder,
                               CelValue::MessageWrapper, absl::Duration,
                               absl::Time, const CelList*, const CelMap*,
                               const CelError*>::Create("dummy_func", false,
                                                        func)));
  auto descriptor = cel_func->descriptor();

  EXPECT_EQ(descriptor.receiver_style(), false);
  EXPECT_EQ(descriptor.name(), "dummy_func");

  int pos = 0;
  ASSERT_EQ(descriptor.types()[pos++], CelValue::Type::kBool);
  ASSERT_EQ(descriptor.types()[pos++], CelValue::Type::kInt64);
  ASSERT_EQ(descriptor.types()[pos++], CelValue::Type::kUint64);
  ASSERT_EQ(descriptor.types()[pos++], CelValue::Type::kDouble);
  ASSERT_EQ(descriptor.types()[pos++], CelValue::Type::kString);
  ASSERT_EQ(descriptor.types()[pos++], CelValue::Type::kBytes);
  ASSERT_EQ(descriptor.types()[pos++], CelValue::Type::kMessage);
  ASSERT_EQ(descriptor.types()[pos++], CelValue::Type::kDuration);
  ASSERT_EQ(descriptor.types()[pos++], CelValue::Type::kTimestamp);
  ASSERT_EQ(descriptor.types()[pos++], CelValue::Type::kList);
  ASSERT_EQ(descriptor.types()[pos++], CelValue::Type::kMap);
  ASSERT_EQ(descriptor.types()[pos++], CelValue::Type::kError);
}

}  // namespace

}  // namespace google::api::expr::runtime
