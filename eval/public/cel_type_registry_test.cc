#include "eval/public/cel_type_registry.h"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "google/protobuf/struct.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "base/types/enum_type.h"
#include "base/values/type_value.h"
#include "eval/public/structs/legacy_type_provider.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::EnumType;
using ::cel::Handle;
using ::cel::MemoryManager;
using ::cel::TypeValue;
using testing::AllOf;
using testing::Contains;
using testing::Eq;
using testing::IsEmpty;
using testing::Key;
using testing::Optional;
using testing::Pair;
using testing::Truly;
using testing::UnorderedElementsAre;
using cel::internal::IsOkAndHolds;
using cel::internal::StatusIs;

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
  const Handle<cel::EnumType>& enum_type = arg;

  if (enum_type->constant_count() != desc->value_count()) {
    return false;
  }

  auto iter_or = enum_type->NewConstantIterator(MemoryManager::Global());
  if (!iter_or.ok()) {
    return false;
  }

  auto iter = std::move(iter_or).value();

  for (int i = 0; i < desc->value_count(); i++) {
    absl::StatusOr<EnumType::Constant> constant = iter->Next();
    if (!constant.ok()) {
      return false;
    }

    const auto* value_desc = desc->value(i);

    if (value_desc->name() != constant->name) {
      return false;
    }
    if (value_desc->number() != constant->number) {
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

    EXPECT_THAT(registry.ListResolveableEnums(),
                UnorderedElementsAre("google.protobuf.NullValue"));
  }
};

// Full proto runtime version.
template <typename T>
struct RegisterEnumDescriptorTestT<
    T, typename std::enable_if<std::is_base_of_v<google::protobuf::Message, T>>::type> {
  void Test() {
    CelTypeRegistry registry;
    registry.Register(google::protobuf::GetEnumDescriptor<TestMessage::TestEnum>());

    EXPECT_THAT(
        registry.ListResolveableEnums(),
        UnorderedElementsAre("google.protobuf.NullValue",
                             "google.api.expr.runtime.TestMessage.TestEnum"));

    EXPECT_THAT(
        registry.resolveable_enums(),
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

  EXPECT_THAT(registry.resolveable_enums(),
              Contains(Pair(
                  "google.api.expr.runtime.TestMessage.TestEnum",
                  testing::Truly([](const Handle<EnumType>& enum_type) {
                    auto constant =
                        enum_type->FindConstantByName("TEST_ENUM_2");
                    return enum_type->name() ==
                               "google.api.expr.runtime.TestMessage.TestEnum" &&
                           constant.value()->number == 20;
                  }))));
}

MATCHER_P(ConstantIntValue, x, "") {
  const EnumType::Constant& constant = arg;

  return constant.number == x;
}

MATCHER_P(ConstantName, x, "") {
  const EnumType::Constant& constant = arg;

  return constant.name == x;
}

TEST(CelTypeRegistryTest, ImplementsEnumType) {
  CelTypeRegistry registry;
  registry.RegisterEnum("google.api.expr.runtime.TestMessage.TestEnum",
                        {
                            {"TEST_ENUM_UNSPECIFIED", 0},
                            {"TEST_ENUM_1", 10},
                            {"TEST_ENUM_2", 20},
                            {"TEST_ENUM_3", 30},
                        });

  ASSERT_THAT(registry.resolveable_enums(),
              Contains(Key("google.api.expr.runtime.TestMessage.TestEnum")));

  const Handle<EnumType>& enum_type = registry.resolveable_enums().at(
      "google.api.expr.runtime.TestMessage.TestEnum");

  EXPECT_TRUE(enum_type->Is<EnumType>());

  EXPECT_THAT(enum_type->FindConstantByName("TEST_ENUM_UNSPECIFIED"),
              IsOkAndHolds(Optional(ConstantIntValue(0))));
  EXPECT_THAT(enum_type->FindConstantByName("TEST_ENUM_1"),
              IsOkAndHolds(Optional(ConstantIntValue(10))));
  EXPECT_THAT(enum_type->FindConstantByName("TEST_ENUM_4"),
              IsOkAndHolds(Eq(absl::nullopt)));

  EXPECT_THAT(enum_type->FindConstantByNumber(20),
              IsOkAndHolds(Optional(ConstantName("TEST_ENUM_2"))));
  EXPECT_THAT(enum_type->FindConstantByNumber(30),
              IsOkAndHolds(Optional(ConstantName("TEST_ENUM_3"))));
  EXPECT_THAT(enum_type->FindConstantByNumber(42),
              IsOkAndHolds(Eq(absl::nullopt)));

  std::vector<std::string> names;
  ASSERT_OK_AND_ASSIGN(auto iter,
                       enum_type->NewConstantIterator(MemoryManager::Global()));
  while (iter->HasNext()) {
    ASSERT_OK_AND_ASSIGN(absl::string_view name, iter->NextName());
    names.push_back(std::string(name));
  }

  EXPECT_THAT(names,
              UnorderedElementsAre("TEST_ENUM_UNSPECIFIED", "TEST_ENUM_1",
                                   "TEST_ENUM_2", "TEST_ENUM_3"));
  EXPECT_THAT(iter->NextName(),
              StatusIs(absl::StatusCode::kFailedPrecondition));

  std::vector<int> numbers;
  ASSERT_OK_AND_ASSIGN(iter,
                       enum_type->NewConstantIterator(MemoryManager::Global()));
  while (iter->HasNext()) {
    ASSERT_OK_AND_ASSIGN(numbers.emplace_back(), iter->NextNumber());
  }

  EXPECT_THAT(numbers, UnorderedElementsAre(0, 10, 20, 30));
  EXPECT_THAT(iter->NextNumber(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(CelTypeRegistryTest, TestRegisterBuiltInEnum) {
  CelTypeRegistry registry;

  ASSERT_THAT(registry.resolveable_enums(),
              Contains(Key("google.protobuf.NullValue")));
  EXPECT_THAT(registry.resolveable_enums()
                  .at("google.protobuf.NullValue")
                  ->FindConstantByName("NULL_VALUE"),
              IsOkAndHolds(Optional(Truly(
                  [](const EnumType::Constant& c) { return c.number == 0; }))));
}

TEST(CelTypeRegistryTest, TestRegisterTypeName) {
  CelTypeRegistry registry;

  // Register the type, scoping the type name lifecycle to the nested block.
  {
    std::string custom_type = "custom_type";
    registry.Register(custom_type);
  }

  auto type = registry.FindType("custom_type");
  ASSERT_TRUE(type);
  EXPECT_TRUE(type->Is<TypeValue>());
  EXPECT_THAT(type.As<TypeValue>()->name(), Eq("custom_type"));
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
  ASSERT_TRUE(type);
  EXPECT_TRUE(type->Is<TypeValue>());
  EXPECT_THAT(type.As<TypeValue>()->name(), Eq("int"));
}

TEST(CelTypeRegistryTest, TestFindTypeAdapterTypeFound) {
  CelTypeRegistry registry;
  registry.RegisterTypeProvider(std::make_unique<TestTypeProvider>(
      std::vector<std::string>{"google.protobuf.Int64"}));
  registry.RegisterTypeProvider(std::make_unique<TestTypeProvider>(
      std::vector<std::string>{"google.protobuf.Any"}));
  auto type = registry.FindType("google.protobuf.Any");
  ASSERT_TRUE(type);
  EXPECT_TRUE(type->Is<TypeValue>());
  EXPECT_THAT(type.As<TypeValue>()->name(), Eq("google.protobuf.Any"));
}

TEST(CelTypeRegistryTest, TestFindTypeNotRegisteredTypeNotFound) {
  CelTypeRegistry registry;
  auto type = registry.FindType("missing.MessageType");
  EXPECT_FALSE(type);
}

}  // namespace

}  // namespace google::api::expr::runtime
