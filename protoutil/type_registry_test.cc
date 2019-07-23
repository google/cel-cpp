#include "protoutil/type_registry.h"

#include "google/protobuf/struct.pb.h"
#include "google/type/money.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "common/value.h"
#include "internal/ref_countable.h"
#include "testutil/util.h"

namespace google {
namespace api {
namespace expr {
namespace protoutil {
namespace {

using testutil::EqualsProto;

// A base object that provides dummy impls for many required overrides.
struct BaseObject : common::Object {
  common::Type object_type() const override {
    return common::Type(common::ObjectType::For<google::type::Money>());
  };

  common::Value GetMember(absl::string_view name) const override {
    return common::Value::NullValue();
  }

  inline google::rpc::Status ForEach(
      const std::function<google::rpc::Status(
          absl::string_view, const common::Value&)>& call) const final {
    return internal::OkStatus();
  }

  /** Serialize the object to protobuf any. */
  void To(google::protobuf::Any* value) const override {}

  bool EqualsImpl(const common::Object& same_type) const override {
    return true;
  }

  // Expose SelfRef for testing.
  common::ParentRef SelfRefProvider() const {
    return common::Object::SelfRefProvider();
  }
};

// An impl that holds a copy.
struct MoneyCopyImpl final : BaseObject {
  explicit MoneyCopyImpl(const google::type::Money& value)
      : value(value), moved(false) {}
  explicit MoneyCopyImpl(google::type::Money&& value)
      : value(std::move(value)), moved(true) {}

  bool owns_value() const override { return true; }

  google::type::Money value;
  bool moved;
};

// An impl that holds a owned pointer.
struct MoneyOwnedImpl final : BaseObject {
  explicit MoneyOwnedImpl(std::unique_ptr<google::type::Money> value)
      : value(std::move(value)) {}

  std::unique_ptr<google::type::Money> value;

  bool owns_value() const override { return true; }
};

// An impl that holds a raw pointer.
struct MoneyUnownedImpl final : BaseObject {
  explicit MoneyUnownedImpl(const google::type::Money* value) : value(value) {}
  explicit MoneyUnownedImpl(const google::type::Money* value,
                            const common::RefProvider& parent)
      : value(value), parent(parent.GetRef()) {}

  const google::type::Money* value;
  common::ValueRef parent;

  bool owns_value() const override { return parent; }
};

TEST(TypeRegistry, Copy) {
  google::type::Money expected_value;
  expected_value.set_nanos(1);

  TypeRegistry reg;
  auto result = reg.RegisterClass<google::type::Money, MoneyCopyImpl>();
  EXPECT_EQ(2, result);

  auto test_value = [&expected_value](common::Value value, bool moved) {
    ASSERT_NE(value.get_if<MoneyCopyImpl>(), nullptr) << value;
    // Always owns value.
    EXPECT_TRUE(value.owns_value());
    // Should have used move constructor if expected.
    EXPECT_EQ(value.get_if<MoneyCopyImpl>()->moved, moved);
    // Should have the right value.
    EXPECT_THAT(value.get_if<MoneyCopyImpl>()->value,
                EqualsProto(expected_value));
  };

  // by const ref.
  test_value(reg.ValueFrom(expected_value), false);
  // by move.
  test_value(reg.ValueFrom(google::type::Money(expected_value)), true);
  // by owned ptr.
  test_value(
      reg.ValueFrom(absl::make_unique<google::type::Money>(expected_value)),
      true);
  // by unowned ptr.
  test_value(reg.ValueFor(&expected_value), false);
  // by parent owned ptr.

  auto parent =
      internal::MakeReffed<MoneyCopyImpl>(google::type::Money(expected_value));
  test_value(reg.ValueFor(&expected_value, parent->SelfRefProvider()), false);
}

TEST(TypeRegistry, Owned) {
  google::type::Money expected_value;
  expected_value.set_nanos(1);
  MoneyOwnedImpl expected_object(
      absl::make_unique<google::type::Money>(expected_value));

  TypeRegistry reg;
  auto result = reg.RegisterClass<google::type::Money, MoneyOwnedImpl>();
  EXPECT_EQ(1, result);

  auto test_value = [&expected_value](common::Value value) {
    ASSERT_NE(value.get_if<MoneyOwnedImpl>(), nullptr) << value;
    // Always owns the value.
    EXPECT_TRUE(value.owns_value());
    // Should have the right value.
    EXPECT_THAT(*value.get_if<MoneyOwnedImpl>()->value,
                EqualsProto(expected_value));
  };

  // by const ref.
  test_value(reg.ValueFrom(expected_value));
  // by move.
  test_value(reg.ValueFrom(google::type::Money(expected_value)));
  // by owned ptr.
  test_value(
      reg.ValueFrom(absl::make_unique<google::type::Money>(expected_value)));
  // by unowned ptr.
  test_value(reg.ValueFor(&expected_value));
  // by parent owned ptr.
  auto parent = internal::MakeReffed<MoneyCopyImpl>(expected_value);
  test_value(reg.ValueFor(&expected_value, parent->SelfRefProvider()));
}

TEST(TypeRegistry, Unowned) {
  google::type::Money expected_value;
  expected_value.set_nanos(1);
  MoneyUnownedImpl expected_object(&expected_value);

  TypeRegistry reg;
  auto result = reg.RegisterClass<google::type::Money, MoneyUnownedImpl>();
  EXPECT_EQ(2, result);

  auto test_value = [&expected_value](common::Value value, bool has_parent) {
    ASSERT_NE(value.get_if<MoneyUnownedImpl>(), nullptr) << value;
    // Transitively owns the value when it holds a reference to a parent.
    EXPECT_EQ(value.owns_value(), has_parent);
    EXPECT_EQ(value.get_if<MoneyUnownedImpl>()->parent, has_parent);
    // Should point to the original value.
    EXPECT_EQ(value.get_if<MoneyUnownedImpl>()->value, &expected_value);
  };

  common::Value error = common::Value::FromError(
      internal::InternalError("Missing callback for google.type.Money"));

  // All From* functions error, as no from constructor is provided.
  // by const ref.
  EXPECT_EQ(reg.ValueFrom(expected_value), error);
  // by move.
  EXPECT_EQ(reg.ValueFrom(google::type::Money(expected_value)), error);
  // by owned ptr.
  EXPECT_EQ(
      reg.ValueFrom(absl::make_unique<google::type::Money>(expected_value)),
      error);

  // by unowned ptr.
  test_value(reg.ValueFor(&expected_value), false);
  // by parent owned ptr.
  // Ignores parent because parent doesn't own its value.
  auto parent1 = internal::MakeReffed<MoneyUnownedImpl>(&expected_value);
  EXPECT_FALSE(parent1->owns_value());
  test_value(reg.ValueFor(&expected_value, parent1->SelfRefProvider()), false);
  auto parent2 = internal::MakeReffed<MoneyCopyImpl>(expected_value);
  EXPECT_TRUE(parent2->owns_value());
  test_value(reg.ValueFor(&expected_value, parent2->SelfRefProvider()), true);
}

// Test that multiple impls can be registered for the same type.
TEST(TypeRegistry, All) {
  TypeRegistry reg;
  int result = 0;
  result += reg.RegisterClass<google::type::Money, MoneyCopyImpl>();
  result += reg.RegisterClass<google::type::Money, MoneyOwnedImpl>();
  result += reg.RegisterClass<google::type::Money, MoneyUnownedImpl>();
  EXPECT_EQ(5, result);

  google::type::Money expected_value;
  expected_value.set_nanos(1);
  MoneyUnownedImpl expected_object(&expected_value);

  // by const ref.
  EXPECT_NE(nullptr, reg.ValueFrom(expected_value).get_if<MoneyCopyImpl>());
  // by move.
  EXPECT_NE(nullptr, reg.ValueFrom(google::type::Money(expected_value))
                         .get_if<MoneyCopyImpl>());
  // by owned ptr.
  EXPECT_NE(
      nullptr,
      reg.ValueFrom(absl::make_unique<google::type::Money>(expected_value))
          .get_if<MoneyOwnedImpl>());
  // by unowned ptr.
  EXPECT_NE(nullptr, reg.ValueFor(&expected_value).get_if<MoneyUnownedImpl>());
  // by parent owned ptr.
  auto parent = internal::MakeReffed<MoneyCopyImpl>(expected_value);
  EXPECT_NE(nullptr, reg.ValueFor(&expected_value, parent->SelfRefProvider())
                         .get_if<MoneyUnownedImpl>());
}

// Test that enums can be customized.
TEST(TypeRegistry, Enum) {
  TypeRegistry reg;
  google::protobuf::Value val;
  auto actual = reg.ValueFrom(val).object_value().GetMember("null_value");
  EXPECT_EQ(actual, common::Value::FromInt(0));
  reg.RegisterConstructor(
      common::EnumType(google::protobuf::NullValue_descriptor()),
      [](common::EnumType, int32_t) { return common::Value::NullValue(); });
  actual = reg.ValueFrom(val).object_value().GetMember("null_value");
  EXPECT_EQ(actual, common::Value::NullValue());
}

// Test that unknown types work.
TEST(TypeRegistry, UnknownAny) {
  TypeRegistry reg;
  google::protobuf::Any unknown;
  unknown.set_type_url("bad_type");
  unknown.set_value("hello");

  auto value = reg.ValueFrom(unknown);
  EXPECT_EQ(value.GetType(), common::Value::FromType("bad_type"));
  EXPECT_EQ(value.object_value().GetMember("bye"),
            common::Value::FromError(internal::UnknownType("bad_type")));

  // Equal to itself.
  EXPECT_EQ(value.hash_code(), reg.ValueFrom(unknown).hash_code());
  EXPECT_EQ(value, reg.ValueFrom(unknown));

  // Not equal to other.
  google::protobuf::Any other_unknown;
  other_unknown.set_type_url("bad_type");
  other_unknown.set_value("bye");
  EXPECT_NE(value.hash_code(), reg.ValueFrom(other_unknown).hash_code());
  EXPECT_NE(value, reg.ValueFrom(other_unknown));

  // Round trips losslessly.
  google::protobuf::Any actual;
  value.object_value().To(&actual);
  EXPECT_THAT(actual, EqualsProto(unknown));
}

// Test that known types work.
TEST(TypeRegistry, KnownAny) {
  TypeRegistry reg;
  google::type::Money money;
  money.set_nanos(100);
  google::protobuf::Any known;
  known.PackFrom(money);

  auto value = reg.ValueFrom(known);
  EXPECT_EQ(value.GetType(), common::Value::FromType("google.type.Money"));
  EXPECT_EQ(value.object_value().GetMember("nanos"),
            common::Value::FromInt(100));

  // Equal to itself.
  EXPECT_EQ(value.hash_code(), reg.ValueFrom(known).hash_code());
  EXPECT_EQ(value, reg.ValueFrom(known));

  // Not equal to other.
  google::protobuf::Any other_known;
  money.set_units(10);
  other_known.PackFrom(money);
  EXPECT_NE(value.hash_code(), reg.ValueFrom(other_known).hash_code());
  EXPECT_NE(value, reg.ValueFrom(other_known));

  // Round trips losslessly.
  google::protobuf::Any actual;
  value.object_value().To(&actual);
  EXPECT_THAT(actual, EqualsProto(known));
}

}  // namespace
}  // namespace protoutil
}  // namespace expr
}  // namespace api
}  // namespace google
