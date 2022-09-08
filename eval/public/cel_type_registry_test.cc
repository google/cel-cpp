#include "eval/public/cel_type_registry.h"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "google/protobuf/struct.pb.h"
#include "google/protobuf/message.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/legacy_type_provider.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

namespace {

using testing::AllOf;
using testing::Contains;
using testing::Eq;
using testing::IsEmpty;
using testing::Key;
using testing::Pair;
using testing::UnorderedElementsAre;

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

MATCHER_P(MatchesEnumDescriptor, desc, "") {
  const std::vector<CelTypeRegistry::Enumerator>& enumerators = arg;

  if (enumerators.size() != desc->value_count()) {
    return false;
  }

  for (int i = 0; i < desc->value_count(); i++) {
    const auto* value_desc = desc->value(i);
    const auto& enumerator = enumerators[i];

    if (value_desc->name() != enumerator.name) {
      return false;
    }
    if (value_desc->number() != enumerator.number) {
      return false;
    }
  }
  return true;
}

MATCHER_P2(EqualsEnumerator, name, number, "") {
  const CelTypeRegistry::Enumerator& enumerator = arg;
  return enumerator.name == name && enumerator.number == number;
}

// Portable build version.
// Full template specification. Default in case of substitution failure below.
template <typename T, typename U = void>
struct RegisterEnumDescriptorTestT {
  void Test() {
    // Portable version doesn't support registering at this time.
    CelTypeRegistry registry;

    EXPECT_THAT(registry.Enums(), IsEmpty());
  }
};

// Full proto runtime version.
template <typename T>
struct RegisterEnumDescriptorTestT<
    T, typename std::enable_if<std::is_base_of_v<google::protobuf::Message, T>>::type> {
  void Test() {
    CelTypeRegistry registry;
    registry.Register(google::protobuf::GetEnumDescriptor<TestMessage::TestEnum>());

    absl::flat_hash_set<std::string> enum_set;
    for (auto enum_desc : registry.Enums()) {
      enum_set.insert(enum_desc->full_name());
    }
    absl::flat_hash_set<std::string> expected_set{
        "google.protobuf.NullValue",
        "google.api.expr.runtime.TestMessage.TestEnum"};
    EXPECT_THAT(enum_set, Eq(expected_set));

    EXPECT_THAT(
        registry.enums_map(),
        AllOf(
            Contains(Pair(
                "google.protobuf.NullValue",
                MatchesEnumDescriptor(
                    google::protobuf::GetEnumDescriptor<google::protobuf::NullValue>()))),
            Contains(Pair(
                "google.api.expr.runtime.TestMessage.TestEnum",
                MatchesEnumDescriptor(
                    google::protobuf::GetEnumDescriptor<TestMessage::TestEnum>())))));
  }
};

using RegisterEnumDescriptorTest = RegisterEnumDescriptorTestT<TestMessage>;

TEST(CelTypeRegistryTest, RegisterEnumDescriptor) {
  RegisterEnumDescriptorTest().Test();
}

TEST(CelTypeRegistryTest, RegisterEnum) {
  CelTypeRegistry registry;
  registry.RegisterEnum("google.api.expr.runtime.TestMessage.TestEnum",
                        {
                            {"TEST_ENUM_UNSPECIFIED", 0},
                            {"TEST_ENUM_1", 10},
                            {"TEST_ENUM_2", 20},
                            {"TEST_ENUM_3", 30},
                        });

  EXPECT_THAT(
      registry.enums_map(),
      Contains(Pair("google.api.expr.runtime.TestMessage.TestEnum",
                    Contains(testing::Truly(
                        [](const CelTypeRegistry::Enumerator& enumerator) {
                          return enumerator.name == "TEST_ENUM_2" &&
                                 enumerator.number == 20;
                        })))));
}

TEST(CelTypeRegistryTest, TestRegisterBuiltInEnum) {
  CelTypeRegistry registry;

  ASSERT_THAT(registry.enums_map(), Contains(Key("google.protobuf.NullValue")));
  EXPECT_THAT(registry.enums_map().at("google.protobuf.NullValue"),
              UnorderedElementsAre(EqualsEnumerator("NULL_VALUE", 0)));
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

TEST(CelTypeRegistryTest, TestFindTypeAdapterTypeFound) {
  CelTypeRegistry registry;
  registry.RegisterTypeProvider(std::make_unique<TestTypeProvider>(
      std::vector<std::string>{"google.protobuf.Int64"}));
  registry.RegisterTypeProvider(std::make_unique<TestTypeProvider>(
      std::vector<std::string>{"google.protobuf.Any"}));
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
