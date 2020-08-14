#include "eval/public/cel_function_registry.h"

#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_function_provider.h"
#include "base/status_macros.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {
namespace {

using testing::Eq;
using testing::Property;
using testing::SizeIs;

class NullLazyFunctionProvider : public virtual CelFunctionProvider {
 public:
  NullLazyFunctionProvider() {}
  // Just return nullptr indicating no matching function.
  absl::StatusOr<const CelFunction*> GetFunction(
      const CelFunctionDescriptor& desc,
      const BaseActivation& activation) const override {
    return nullptr;
  }
};

class ConstCelFunction : public CelFunction {
 public:
  ConstCelFunction() : CelFunction(MakeDescriptor()) {}
  explicit ConstCelFunction(const CelFunctionDescriptor& desc)
      : CelFunction(desc) {}

  static CelFunctionDescriptor MakeDescriptor() {
    return {"ConstFunction", false, {}};
  }

  absl::Status Evaluate(absl::Span<const CelValue> args, CelValue* output,
                        google::protobuf::Arena* arena) const override {
    *output = CelValue::CreateInt64(42);

    return absl::OkStatus();
  }
};

TEST(CelFunctionRegistryTest, InsertAndRetrieveLazyFunction) {
  CelFunctionDescriptor lazy_function_desc{"LazyFunction", false, {}};
  CelFunctionRegistry registry;
  Activation activation;
  auto register_status = registry.RegisterLazyFunction(
      lazy_function_desc, std::make_unique<NullLazyFunctionProvider>());
  EXPECT_OK(register_status);

  const auto providers = registry.FindLazyOverloads("LazyFunction", false, {});
  EXPECT_THAT(providers, testing::SizeIs(1));
  auto func = providers[0]->GetFunction(lazy_function_desc, activation);
  ASSERT_OK(func.status());
  EXPECT_THAT(func.value(), Eq(nullptr));
}

// Confirm that lazy and static functions share the same descriptor space:
// i.e. you can't insert both a lazy function and a static function for the same
// descriptors.
TEST(CelFunctionRegistryTest, LazyAndStaticFunctionShareDescriptorSpace) {
  CelFunctionRegistry registry;
  CelFunctionDescriptor desc = ConstCelFunction::MakeDescriptor();
  auto register_status = registry.RegisterLazyFunction(
      desc, std::make_unique<NullLazyFunctionProvider>());
  EXPECT_OK(register_status);

  absl::Status status = registry.Register(std::make_unique<ConstCelFunction>());
  EXPECT_FALSE(status.ok());
}

TEST(CelFunctionRegistryTest, ListFunctions) {
  CelFunctionDescriptor lazy_function_desc{"LazyFunction", false, {}};
  CelFunctionRegistry registry;

  auto register_status = registry.RegisterLazyFunction(
      lazy_function_desc, std::make_unique<NullLazyFunctionProvider>());
  EXPECT_OK(register_status);
  EXPECT_OK(registry.Register(std::make_unique<ConstCelFunction>()));

  auto registered_functions = registry.ListFunctions();

  EXPECT_THAT(registered_functions, SizeIs(2));
  EXPECT_THAT(registered_functions["LazyFunction"], SizeIs(1));
  EXPECT_THAT(registered_functions["ConstFunction"], SizeIs(1));
}

TEST(CelFunctionRegistryTest, DefaultLazyProvider) {
  CelFunctionDescriptor lazy_function_desc{"LazyFunction", false, {}};
  CelFunctionRegistry registry;
  Activation activation;
  EXPECT_OK(registry.RegisterLazyFunction(lazy_function_desc));
  auto insert_status = activation.InsertFunction(
      std::make_unique<ConstCelFunction>(lazy_function_desc));
  EXPECT_OK(insert_status);

  const auto providers = registry.FindLazyOverloads("LazyFunction", false, {});
  EXPECT_THAT(providers, testing::SizeIs(1));
  auto func = providers[0]->GetFunction(lazy_function_desc, activation);
  ASSERT_OK(func.status());
  EXPECT_THAT(func.value(), Property(&CelFunction::descriptor,
                                     Property(&CelFunctionDescriptor::name,
                                              Eq("LazyFunction"))));
}

}  // namespace
}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
