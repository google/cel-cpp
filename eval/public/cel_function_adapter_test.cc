#include "eval/public/cel_function_adapter.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

TEST(CelFunctionAdapterTest, TestAdapterNoArg) {
  auto func = [](google::protobuf::Arena*) -> int64_t { return 100; };

  auto func_status = FunctionAdapter<int64_t>::Create("const", false, func);

  ASSERT_TRUE(func_status.ok());

  auto cel_func = std::move(func_status.ValueOrDie());

  absl::Span<CelValue> args;

  CelValue result = CelValue::CreateNull();
  google::protobuf::Arena arena;
  auto eval_status = cel_func->Evaluate(args, &result, &arena);

  ASSERT_TRUE(eval_status.ok());

  ASSERT_TRUE(
      result.IsInt64());  // Obvious failure, for educational purposes only.
}

TEST(CelFunctionAdapterTest, TestAdapterOneArg) {
  std::function<int64_t(google::protobuf::Arena*, int64_t)> func =
      [](google::protobuf::Arena* arena, int64_t i) -> int64_t { return i + 1; };

  auto func_status = FunctionAdapter<int64_t, int64_t>::Create("_++_", false, func);

  ASSERT_TRUE(func_status.ok());

  auto cel_func = std::move(func_status.ValueOrDie());

  std::vector<CelValue> args_vec;
  args_vec.push_back(CelValue::CreateInt64(99));

  CelValue result = CelValue::CreateNull();
  google::protobuf::Arena arena;

  absl::Span<CelValue> args(&args_vec[0], args_vec.size());

  auto eval_status = cel_func->Evaluate(args, &result, &arena);

  ASSERT_TRUE(eval_status.ok());

  ASSERT_TRUE(result.IsInt64());
  EXPECT_EQ(result.Int64OrDie(), 100);
}

TEST(CelFunctionAdapterTest, TestAdapterTwoArgs) {
  auto func = [](google::protobuf::Arena* arena, int64_t i, int64_t j) -> int64_t {
    return i + j;
  };

  auto func_status =
      FunctionAdapter<int64_t, int64_t, int64_t>::Create("_++_", false, func);

  ASSERT_TRUE(func_status.ok());

  auto cel_func = std::move(func_status.ValueOrDie());

  std::vector<CelValue> args_vec;
  args_vec.push_back(CelValue::CreateInt64(20));
  args_vec.push_back(CelValue::CreateInt64(22));

  CelValue result = CelValue::CreateNull();
  google::protobuf::Arena arena;

  absl::Span<CelValue> args(&args_vec[0], args_vec.size());

  auto eval_status = cel_func->Evaluate(args, &result, &arena);

  ASSERT_TRUE(eval_status.ok());

  ASSERT_TRUE(result.IsInt64());
  EXPECT_EQ(result.Int64OrDie(), 42);
}

using StringHolder = CelValue::StringHolder;

TEST(CelFunctionAdapterTest, TestAdapterThreeArgs) {
  auto func = [](google::protobuf::Arena* arena, StringHolder s1, StringHolder s2,
                 StringHolder s3) -> StringHolder {
    std::string value = absl::StrCat(s1.value(), s2.value(), s3.value());

    return StringHolder(google::protobuf::Arena::Create<std::string>(arena, std::move(value)));
  };

  auto func_status =
      FunctionAdapter<StringHolder, StringHolder, StringHolder,
                      StringHolder>::Create("concat", false, func);

  ASSERT_TRUE(func_status.ok());

  auto cel_func = std::move(func_status.ValueOrDie());

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

  auto eval_status = cel_func->Evaluate(args, &result, &arena);

  ASSERT_TRUE(eval_status.ok());

  ASSERT_TRUE(result.IsString());
  EXPECT_EQ(result.StringOrDie().value(), "123");
}

TEST(CelFunctionAdapterTest, TestTypeDeductionForCelValueBasicTypes) {
  auto func = [](google::protobuf::Arena* arena, bool, int64_t, uint64_t, double,
                 CelValue::StringHolder, CelValue::BytesHolder,
                 const google::protobuf::Message*, absl::Duration, absl::Time,
                 const CelList*, const CelMap*,
                 const CelError*) -> bool { return false; };

  auto func_status =
      FunctionAdapter<bool, bool, int64_t, uint64_t, double, CelValue::StringHolder,
                      CelValue::BytesHolder, const google::protobuf::Message*,
                      absl::Duration, absl::Time, const CelList*, const CelMap*,
                      const CelError*>::Create("dummy_func", false, func);

  ASSERT_TRUE(func_status.ok());

  auto cel_func = std::move(func_status.ValueOrDie());

  auto descriptor = cel_func->descriptor();

  EXPECT_EQ(descriptor.receiver_style, false);
  EXPECT_EQ(descriptor.name, "dummy_func");

  int pos = 0;
  ASSERT_EQ(descriptor.types[pos++], CelValue::Type::kBool);
  ASSERT_EQ(descriptor.types[pos++], CelValue::Type::kInt64);
  ASSERT_EQ(descriptor.types[pos++], CelValue::Type::kUint64);
  ASSERT_EQ(descriptor.types[pos++], CelValue::Type::kDouble);
  ASSERT_EQ(descriptor.types[pos++], CelValue::Type::kString);
  ASSERT_EQ(descriptor.types[pos++], CelValue::Type::kBytes);
  ASSERT_EQ(descriptor.types[pos++], CelValue::Type::kMessage);
  ASSERT_EQ(descriptor.types[pos++], CelValue::Type::kDuration);
  ASSERT_EQ(descriptor.types[pos++], CelValue::Type::kTimestamp);
  ASSERT_EQ(descriptor.types[pos++], CelValue::Type::kList);
  ASSERT_EQ(descriptor.types[pos++], CelValue::Type::kMap);
  ASSERT_EQ(descriptor.types[pos++], CelValue::Type::kError);
}

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
