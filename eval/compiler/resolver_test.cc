#include "eval/compiler/resolver.h"

#include <memory>

#include "absl/status/status.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_type_registry.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/status_macros.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

namespace {

using testing::Eq;

class FakeFunction : public CelFunction {
 public:
  explicit FakeFunction(const std::string& name)
      : CelFunction(CelFunctionDescriptor{name, false, {}}) {}

  absl::Status Evaluate(absl::Span<const CelValue> args, CelValue* result,
                        google::protobuf::Arena* arena) const override {
    return absl::OkStatus();
  }
};

TEST(ResolverTest, TestFullyQualifiedNames) {
  CelFunctionRegistry func_registry;
  CelTypeRegistry type_registry;
  Resolver resolver("google.api.expr", &func_registry, &type_registry);

  auto names = resolver.FullyQualifiedNames("simple_name");
  std::vector<std::string> expected_names(
      {"google.api.expr.simple_name", "google.api.simple_name",
       "google.simple_name", "simple_name"});
  EXPECT_THAT(names, Eq(expected_names));
}

TEST(ResolverTest, TestFullyQualifiedNamesPartiallyQualifiedName) {
  CelFunctionRegistry func_registry;
  CelTypeRegistry type_registry;
  Resolver resolver("google.api.expr", &func_registry, &type_registry);

  auto names = resolver.FullyQualifiedNames("expr.simple_name");
  std::vector<std::string> expected_names(
      {"google.api.expr.expr.simple_name", "google.api.expr.simple_name",
       "google.expr.simple_name", "expr.simple_name"});
  EXPECT_THAT(names, Eq(expected_names));
}

TEST(ResolverTest, TestFullyQualifiedNamesAbsoluteName) {
  CelFunctionRegistry func_registry;
  CelTypeRegistry type_registry;
  Resolver resolver("google.api.expr", &func_registry, &type_registry);

  auto names = resolver.FullyQualifiedNames(".google.api.expr.absolute_name");
  EXPECT_THAT(names.size(), Eq(1));
  EXPECT_THAT(names[0], Eq("google.api.expr.absolute_name"));
}

TEST(ResolverTest, TestFindConstantEnum) {
  CelFunctionRegistry func_registry;
  CelTypeRegistry type_registry;
  type_registry.Register(TestMessage::TestEnum_descriptor());
  Resolver resolver("google.api.expr.runtime.TestMessage", &func_registry,
                    &type_registry);

  auto enum_value = resolver.FindConstant("TestEnum.TEST_ENUM_1", -1);
  EXPECT_TRUE(enum_value.has_value());
  EXPECT_TRUE(enum_value->IsInt64());
  EXPECT_THAT(enum_value->Int64OrDie(), Eq(1L));

  enum_value = resolver.FindConstant(
      ".google.api.expr.runtime.TestMessage.TestEnum.TEST_ENUM_2", -1);
  EXPECT_TRUE(enum_value.has_value());
  EXPECT_TRUE(enum_value->IsInt64());
  EXPECT_THAT(enum_value->Int64OrDie(), Eq(2L));
}

TEST(ResolverTest, TestFindConstantUnqualifiedType) {
  CelFunctionRegistry func_registry;
  CelTypeRegistry type_registry;
  Resolver resolver("cel", &func_registry, &type_registry);

  auto type_value = resolver.FindConstant("int", -1);
  EXPECT_TRUE(type_value.has_value());
  EXPECT_TRUE(type_value->IsCelType());
  EXPECT_THAT(type_value->CelTypeOrDie().value(), Eq("int"));
}

TEST(ResolverTest, TestFindConstantFullyQualifiedType) {
  CelFunctionRegistry func_registry;
  CelTypeRegistry type_registry;
  Resolver resolver("cel", &func_registry, &type_registry);

  auto type_value =
      resolver.FindConstant(".google.api.expr.runtime.TestMessage", -1);
  EXPECT_TRUE(type_value.has_value());
  EXPECT_TRUE(type_value->IsCelType());
  EXPECT_THAT(type_value->CelTypeOrDie().value(),
              Eq("google.api.expr.runtime.TestMessage"));
}

TEST(ResolverTest, TestFindConstantQualifiedTypeDisabled) {
  CelFunctionRegistry func_registry;
  CelTypeRegistry type_registry;
  Resolver resolver("", &func_registry, &type_registry, false);
  auto type_value =
      resolver.FindConstant(".google.api.expr.runtime.TestMessage", -1);
  EXPECT_FALSE(type_value.has_value());
}

TEST(ResolverTest, TestFindDescriptorBySimpleName) {
  CelFunctionRegistry func_registry;
  CelTypeRegistry type_registry;
  Resolver resolver("google.api.expr.runtime", &func_registry, &type_registry);

  auto desc_value = resolver.FindDescriptor("TestMessage", -1);
  EXPECT_TRUE(desc_value != nullptr);
  EXPECT_THAT(desc_value, Eq(TestMessage::GetDescriptor()));
}

TEST(ResolverTest, TestFindDescriptorByQualifiedName) {
  CelFunctionRegistry func_registry;
  CelTypeRegistry type_registry;
  Resolver resolver("google.api.expr.runtime", &func_registry, &type_registry);

  auto desc_value =
      resolver.FindDescriptor(".google.api.expr.runtime.TestMessage", -1);
  EXPECT_TRUE(desc_value != nullptr);
  EXPECT_THAT(desc_value, Eq(TestMessage::GetDescriptor()));
}

TEST(ResolverTest, TestFindDescriptorNotFound) {
  CelFunctionRegistry func_registry;
  CelTypeRegistry type_registry;
  Resolver resolver("google.api.expr.runtime", &func_registry, &type_registry);

  auto desc_value = resolver.FindDescriptor("UndefinedMessage", -1);
  EXPECT_TRUE(desc_value == nullptr);
}

TEST(ResolverTest, TestFindOverloads) {
  CelFunctionRegistry func_registry;
  auto status =
      func_registry.Register(std::make_unique<FakeFunction>("fake_func"));
  ASSERT_OK(status);
  status = func_registry.Register(
      std::make_unique<FakeFunction>("cel.fake_ns_func"));
  ASSERT_OK(status);

  CelTypeRegistry type_registry;
  Resolver resolver("cel", &func_registry, &type_registry);

  auto overloads =
      resolver.FindOverloads("fake_func", false, ArgumentsMatcher(0));
  EXPECT_THAT(overloads.size(), Eq(1));
  EXPECT_THAT(overloads[0]->descriptor().name(), Eq("fake_func"));

  overloads =
      resolver.FindOverloads("fake_ns_func", false, ArgumentsMatcher(0));
  EXPECT_THAT(overloads.size(), Eq(1));
  EXPECT_THAT(overloads[0]->descriptor().name(), Eq("cel.fake_ns_func"));
}

TEST(ResolverTest, TestFindLazyOverloads) {
  CelFunctionRegistry func_registry;
  auto status = func_registry.RegisterLazyFunction(
      CelFunctionDescriptor{"fake_lazy_func", false, {}});
  ASSERT_OK(status);
  status = func_registry.RegisterLazyFunction(
      CelFunctionDescriptor{"cel.fake_lazy_ns_func", false, {}});
  ASSERT_OK(status);

  CelTypeRegistry type_registry;
  Resolver resolver("cel", &func_registry, &type_registry);

  auto overloads =
      resolver.FindLazyOverloads("fake_lazy_func", false, ArgumentsMatcher(0));
  EXPECT_THAT(overloads.size(), Eq(1));

  overloads = resolver.FindLazyOverloads("fake_lazy_ns_func", false,
                                         ArgumentsMatcher(0));
  EXPECT_THAT(overloads.size(), Eq(1));
}

}  // namespace

}  // namespace google::api::expr::runtime
