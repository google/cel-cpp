#include "eval/public/cel_type_registry.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "base/memory.h"
#include "base/type.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "base/type_provider.h"
#include "base/types/enum_type.h"
#include "base/types/struct_type.h"
#include "base/value_factory.h"
#include "base/values/struct_value.h"
#include "base/values/struct_value_builder.h"
#include "base/values/type_value.h"
#include "common/native_type.h"
#include "eval/public/structs/legacy_type_adapter.h"
#include "eval/public/structs/legacy_type_provider.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::EnumType;
using ::cel::Handle;
using ::cel::MemoryManagerRef;
using ::cel::Type;
using ::cel::TypeFactory;
using ::cel::TypeManager;
using ::cel::TypeProvider;
using ::cel::ValueFactory;
using testing::Contains;
using testing::Eq;
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
  ASSERT_OK_AND_ASSIGN(auto iter, enum_type->NewConstantIterator(
                                      MemoryManagerRef::ReferenceCounting()));
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
  ASSERT_OK_AND_ASSIGN(iter, enum_type->NewConstantIterator(
                                 MemoryManagerRef::ReferenceCounting()));
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

class TestStructType : public cel::base_internal::AbstractStructType {
 public:
  explicit TestStructType(absl::string_view name) : name_(name) {}

  absl::string_view name() const override { return name_; }

  size_t field_count() const override { return 0; }

  absl::StatusOr<absl::optional<Field>> FindFieldByName(
      TypeManager& type_manager, absl::string_view name) const override {
    return absl::nullopt;
  }

  absl::StatusOr<absl::optional<Field>> FindFieldByNumber(
      TypeManager& type_manager, int64_t number) const override {
    return absl::nullopt;
  }

  absl::StatusOr<absl::Nonnull<std::unique_ptr<FieldIterator>>>
  NewFieldIterator(TypeManager& type_manager) const override {
    return absl::UnimplementedError("");
  }

  absl::StatusOr<
      absl::Nonnull<std::unique_ptr<cel::StructValueBuilderInterface>>>
  NewValueBuilder(ValueFactory& value_factory
                      ABSL_ATTRIBUTE_LIFETIME_BOUND) const override {
    return absl::UnimplementedError("");
  }

  cel::NativeTypeId GetNativeTypeId() const override {
    return cel::NativeTypeId::For<TestStructType>();
  }

 private:
  std::string name_;
};

TEST(CelTypeRegistryTest, RegisterModernProvider) {
  CelTypeRegistry registry;

  class ExampleTypeProvider : public TypeProvider {
    absl::StatusOr<absl::optional<Handle<Type>>> ProvideType(
        TypeFactory& factory, absl::string_view name) const override {
      if (name == "custom_type") {
        return factory.CreateStructType<TestStructType>("custom_type");
      }
      return absl::nullopt;
    }
  };

  registry.RegisterModernTypeProvider(std::make_unique<ExampleTypeProvider>());
  TypeFactory type_factory(MemoryManagerRef::ReferenceCounting());
  TypeManager type_manager(type_factory, registry.GetTypeProvider());

  ASSERT_OK_AND_ASSIGN(absl::optional<Handle<Type>> type_value,
                       type_manager.ResolveType("custom_type"));
  ASSERT_TRUE(type_value.has_value());
  EXPECT_EQ((*type_value)->name(), "custom_type");

  ASSERT_OK_AND_ASSIGN(type_value, type_manager.ResolveType("custom_type2"));
  EXPECT_EQ(type_value, absl::nullopt);
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

MATCHER_P(TypeNameIs, name, "") {
  const Handle<Type>& type = arg;
  *result_listener << "got typename: " << type->name();
  return type->name() == name;
}

TEST(CelTypeRegistryTypeProviderTest, Builtins) {
  CelTypeRegistry registry;

  cel::TypeFactory type_factory(MemoryManagerRef::ReferenceCounting());
  cel::TypeManager type_manager(type_factory, registry.GetTypeProvider());

  // simple
  ASSERT_OK_AND_ASSIGN(absl::optional<Handle<Type>> bool_type,
                       type_manager.ResolveType("bool"));
  EXPECT_THAT(bool_type, Optional(TypeNameIs("bool")));
  // opaque
  ASSERT_OK_AND_ASSIGN(absl::optional<Handle<Type>> timestamp_type,
                       type_manager.ResolveType("google.protobuf.Timestamp"));
  EXPECT_THAT(timestamp_type,
              Optional(TypeNameIs("google.protobuf.Timestamp")));
  // wrapper
  ASSERT_OK_AND_ASSIGN(absl::optional<Handle<Type>> int_wrapper_type,
                       type_manager.ResolveType("google.protobuf.Int64Value"));
  EXPECT_THAT(int_wrapper_type,
              Optional(TypeNameIs("google.protobuf.Int64Value")));
  // json
  ASSERT_OK_AND_ASSIGN(absl::optional<Handle<Type>> json_struct_type,
                       type_manager.ResolveType("google.protobuf.Struct"));
  EXPECT_THAT(json_struct_type, Optional(TypeNameIs("map")));
  // special
  ASSERT_OK_AND_ASSIGN(absl::optional<Handle<Type>> any_type,
                       type_manager.ResolveType("google.protobuf.Any"));
  EXPECT_THAT(any_type, Optional(TypeNameIs("google.protobuf.Any")));
}

}  // namespace

}  // namespace google::api::expr::runtime
