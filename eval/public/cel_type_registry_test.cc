#include "eval/public/cel_type_registry.h"

#include "google/protobuf/any.pb.h"
#include "absl/container/flat_hash_map.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

namespace {

using testing::Eq;

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

TEST(CelTypeRegistryTest, TestFindDescriptorFound) {
  CelTypeRegistry registry;
  auto desc = registry.FindDescriptor("google.protobuf.Any");
  ASSERT_TRUE(desc != nullptr);
  EXPECT_THAT(desc->full_name(), Eq("google.protobuf.Any"));
}

TEST(CelTypeRegistryTest, TestFindDescriptorNotFound) {
  CelTypeRegistry registry;
  auto desc = registry.FindDescriptor("missing.MessageType");
  EXPECT_TRUE(desc == nullptr);
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
