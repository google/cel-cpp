#include "eval/public/cel_type_registry.h"

#include <memory>
#include <string>
#include <utility>

#include "google/protobuf/any.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/legacy_type_provider.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

namespace {

using testing::Eq;

class TestTypeProvider : public LegacyTypeProvider {
 public:
  explicit TestTypeProvider(std::vector<std::string> types)
      : types_(std::move(types)) {}

  // Return a type adapter for an opaque type
  // (no reflection operations supported).
  absl::optional<LegacyTypeAdapter> ProvideLegacyType(
      absl::string_view name) const override {
    for (const auto& type : types_) {
      if (name == type) {
        return LegacyTypeAdapter(/*access=*/nullptr, /*mutation=*/nullptr);
      }
    }
    return absl::nullopt;
  }

 private:
  std::vector<std::string> types_;
};

TEST(CelTypeRegistryTest, TestRegisterEnumDescriptor) {
  CelTypeRegistry registry;
  registry.Register(TestMessage::TestEnum_descriptor());

  absl::flat_hash_set<std::string> enum_set;
  for (auto enum_desc : registry.Enums()) {
    enum_set.insert(enum_desc->full_name());
  }
  absl::flat_hash_set<std::string> expected_set;
  expected_set.insert({"google.protobuf.NullValue"});
  expected_set.insert({"google.api.expr.runtime.TestMessage.TestEnum"});
  EXPECT_THAT(enum_set, Eq(expected_set));
}

TEST(CelTypeRegistryTest, TestRegisterTypeName) {
  CelTypeRegistry registry;

  // Register the type, scoping the type name lifecycle to the nested block.
  {
    std::string custom_type = "custom_type";
    registry.Register(custom_type);
  }

  auto type = registry.FindType("custom_type");
  ASSERT_TRUE(type.has_value());
  EXPECT_TRUE(type->IsCelType());
  EXPECT_THAT(type->CelTypeOrDie().value(), Eq("custom_type"));
}

TEST(CelTypeRegistryTest, TestGetFirstTypeProviderSuccess) {
  CelTypeRegistry registry;
  registry.RegisterTypeProvider(std::make_unique<TestTypeProvider>(
      std::vector<std::string>{"google.protobuf.Int64"}));
  registry.RegisterTypeProvider(std::make_unique<TestTypeProvider>(
      std::vector<std::string>{"google.protobuf.Any"}));
  auto type_provider = registry.GetFirstTypeProvider();
  ASSERT_NE(type_provider, nullptr);
  ASSERT_TRUE(
      type_provider->ProvideLegacyType("google.protobuf.Int64").has_value());
  ASSERT_FALSE(
      type_provider->ProvideLegacyType("google.protobuf.Any").has_value());
}

TEST(CelTypeRegistryTest, TestGetFirstTypeProviderFailureOnEmpty) {
  CelTypeRegistry registry;
  auto type_provider = registry.GetFirstTypeProvider();
  ASSERT_EQ(type_provider, nullptr);
}

TEST(CelTypeRegistryTest, TestFindTypeAdapterFound) {
  CelTypeRegistry registry;
  registry.RegisterTypeProvider(std::make_unique<TestTypeProvider>(
      std::vector<std::string>{"google.protobuf.Any"}));
  auto desc = registry.FindTypeAdapter("google.protobuf.Any");
  ASSERT_TRUE(desc.has_value());
}

TEST(CelTypeRegistryTest, TestFindTypeAdapterFoundMultipleProviders) {
  CelTypeRegistry registry;
  registry.RegisterTypeProvider(std::make_unique<TestTypeProvider>(
      std::vector<std::string>{"google.protobuf.Int64"}));
  registry.RegisterTypeProvider(std::make_unique<TestTypeProvider>(
      std::vector<std::string>{"google.protobuf.Any"}));
  auto desc = registry.FindTypeAdapter("google.protobuf.Any");
  ASSERT_TRUE(desc.has_value());
}

TEST(CelTypeRegistryTest, TestFindTypeAdapterNotFound) {
  CelTypeRegistry registry;
  auto desc = registry.FindTypeAdapter("missing.MessageType");
  EXPECT_FALSE(desc.has_value());
}

TEST(CelTypeRegistryTest, TestFindTypeCoreTypeFound) {
  CelTypeRegistry registry;
  auto type = registry.FindType("int");
  ASSERT_TRUE(type.has_value());
  EXPECT_TRUE(type->IsCelType());
  EXPECT_THAT(type->CelTypeOrDie().value(), Eq("int"));
}

TEST(CelTypeRegistryTest, TestFindTypeProtobufTypeFound) {
  CelTypeRegistry registry;
  auto type = registry.FindType("google.protobuf.Any");
  ASSERT_TRUE(type.has_value());
  EXPECT_TRUE(type->IsCelType());
  EXPECT_THAT(type->CelTypeOrDie().value(), Eq("google.protobuf.Any"));
}

TEST(CelTypeRegistryTest, TestFindTypeNotRegisteredTypeNotFound) {
  CelTypeRegistry registry;
  auto type = registry.FindType("missing.MessageType");
  EXPECT_FALSE(type.has_value());
}

}  // namespace

}  // namespace google::api::expr::runtime
