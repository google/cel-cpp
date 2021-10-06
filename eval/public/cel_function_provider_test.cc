#include "eval/public/cel_function_provider.h"

#include "eval/public/activation.h"
#include "internal/status_macros.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

namespace {

using testing::Eq;
using testing::HasSubstr;
using testing::Ne;

class ConstCelFunction : public CelFunction {
 public:
  ConstCelFunction() : CelFunction({"ConstFunction", false, {}}) {}
  explicit ConstCelFunction(const CelFunctionDescriptor& desc)
      : CelFunction(desc) {}
  absl::Status Evaluate(absl::Span<const CelValue> args, CelValue* output,
                        google::protobuf::Arena* arena) const override {
    return absl::Status(absl::StatusCode::kUnimplemented, "Not Implemented");
  }
};

TEST(CreateActivationFunctionProviderTest, NoOverloadFound) {
  Activation activation;
  auto provider = CreateActivationFunctionProvider();

  auto func = provider->GetFunction({"LazyFunc", false, {}}, activation);

  ASSERT_OK(func);
  EXPECT_THAT(*func, Eq(nullptr));
}

TEST(CreateActivationFunctionProviderTest, OverloadFound) {
  Activation activation;
  CelFunctionDescriptor desc{"LazyFunc", false, {}};
  auto provider = CreateActivationFunctionProvider();

  auto status =
      activation.InsertFunction(std::make_unique<ConstCelFunction>(desc));
  EXPECT_OK(status);

  auto func = provider->GetFunction(desc, activation);

  ASSERT_OK(func);
  EXPECT_THAT(*func, Ne(nullptr));
}

TEST(CreateActivationFunctionProviderTest, AmbiguousLookup) {
  Activation activation;
  CelFunctionDescriptor desc1{"LazyFunc", false, {CelValue::Type::kInt64}};
  CelFunctionDescriptor desc2{"LazyFunc", false, {CelValue::Type::kUint64}};
  CelFunctionDescriptor match_desc{"LazyFunc", false, {CelValue::Type::kAny}};

  auto provider = CreateActivationFunctionProvider();

  auto status =
      activation.InsertFunction(std::make_unique<ConstCelFunction>(desc1));
  EXPECT_OK(status);
  status = activation.InsertFunction(std::make_unique<ConstCelFunction>(desc2));
  EXPECT_OK(status);

  auto func = provider->GetFunction(match_desc, activation);

  EXPECT_THAT(std::string(func.status().message()),
              HasSubstr("Couldn't resolve function"));
}

}  // namespace

}  // namespace google::api::expr::runtime
