#include "common/value.h"

#include <cstddef>
#include <utility>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/rpc/status.pb.h"
#include "google/type/money.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/str_cat.h"
#include "common/custom_object.h"
#include "internal/status_util.h"
#include "internal/types.h"
#include "internal/value_internal.h"
#include "testutil/util.h"

namespace google {
namespace api {
namespace expr {
namespace common {
namespace {

using testutil::ExpectSameType;

class DummyObject final : public OpaqueObject {
 public:
  Type object_type() const override {
    return Type(ObjectType(google::type::Money::descriptor()));
  }

  void To(google::protobuf::Any* value) const override {}

  std::string ToString() const override { return Object::ToString(); }

 protected:
  inline bool EqualsImpl(const Object& rhs) const override { return true; }
  inline std::size_t ComputeHash() const override { return 0; }
};

class DummyMap final : public Map {
 public:
  std::size_t size() const override { return 1; }

  google::rpc::Status ForEach(
      const std::function<google::rpc::Status(const Value&, const Value&)>&
          call) const override {
    return call(Value::ForString("key"), Value::ForString("value"));
  }

  bool owns_value() const override { return true; }

 protected:
  Value GetImpl(const Value& key) const override {
    if (key == Value::ForString("key")) return Value::ForString("value");
    return Value::FromError(internal::NotFoundError(""));
  }
};

class DummyList final : public List {
 public:
  std::size_t size() const override { return 1; }
  bool owns_value() const override { return true; }

  google::rpc::Status ForEach(
      const std::function<google::rpc::Status(const Value&)>& call)
      const override {
    return call(Value::ForString("elem"));
  }

  Value Get(std::size_t index) const override {
    if (index == 0) {
      return Value::ForString("elem");
    }
    return Value::FromError(internal::OutOfRangeError(index, 1));
  }
};

struct ValueTestCase {
  Value value;
  Value value_copy;
  Value::Kind kind;
  Value type;
  std::string debug_string;
  std::string type_debug_string;
  bool is_inline;
  bool is_value;
  bool owns_value;

  static Value::Kind GetKind(BasicTypeValue type) {
    switch (type) {
      case BasicTypeValue::kNull:
        return Value::Kind::kNull;
      case BasicTypeValue::kBool:
        return Value::Kind::kBool;
      case BasicTypeValue::kInt:
        return Value::Kind::kInt;
      case BasicTypeValue::kUint:
        return Value::Kind::kUInt;
      case BasicTypeValue::kDouble:
        return Value::Kind::kDouble;
      case BasicTypeValue::kString:
        return Value::Kind::kString;
      case BasicTypeValue::kBytes:
        return Value::Kind::kBytes;
      case BasicTypeValue::kList:
        return Value::Kind::kList;
      case BasicTypeValue::kMap:
        return Value::Kind::kMap;
      case BasicTypeValue::kType:
        return Value::Kind::kType;
      case BasicTypeValue::DO_NOT_USE:  // Force compiler error if switch is not
                                        // completed.
        EXPECT_TRUE(false) << "not a basic type";
    }
    return Value::Kind::kNull;
  }

  static ValueTestCase ForInline(const Value& value,
                                 const std::string& debug_string,
                                 const std::string& type_debug_string) {
    return ValueTestCase{value, value,        value.kind(),
                         value, debug_string, type_debug_string,
                         true,  false,        true};
  }

  static ValueTestCase ForInline(const Value& value, EnumType type,
                                 const std::string& debug_string,
                                 const std::string& type_debug_string) {
    return ValueTestCase{value,
                         value,
                         Value::Kind::kEnum,
                         Value::FromType(type),
                         debug_string,
                         type_debug_string,
                         true,
                         true,
                         true};
  }

  static ValueTestCase ForInline(const Value& value, BasicTypeValue type,
                                 const std::string& debug_string,
                                 const std::string& type_debug_string) {
    return ValueTestCase{value,         value,
                         GetKind(type), Value::FromType(type),
                         debug_string,  type_debug_string,
                         true,          true,
                         true};
  }

  static ValueTestCase ForNonInline(Value value, Value copy,
                                    BasicTypeValue type,
                                    const std::string& debug_string,
                                    const std::string& type_debug_string,
                                    bool owns_value = true) {
    return ValueTestCase{std::move(value),
                         std::move(copy),
                         GetKind(type),
                         Value::FromType(type),
                         debug_string,
                         type_debug_string,
                         false,
                         true,
                         owns_value};
  }

  static ValueTestCase ForNonInline(Value value, Value copy, EnumType type,
                                    const std::string& debug_string,
                                    const std::string& type_debug_string) {
    return ValueTestCase{std::move(value),
                         std::move(copy),
                         Value::Kind::kEnum,
                         Value::FromType(type),
                         debug_string,
                         type_debug_string,
                         false,
                         true,
                         true};
  }
  static ValueTestCase ForNonInline(Value value, Value copy, Value::Kind kind,
                                    absl::string_view full_name,
                                    absl::string_view args) {
    auto type = Value::FromType(value.GetType().type_value().object_type());
    return ValueTestCase{std::move(value),
                         std::move(copy),
                         kind,
                         type,
                         absl::StrCat(full_name, args),
                         std::string(full_name),
                         false,
                         true,
                         true};
  }

  static ValueTestCase ForNonValue(Value value, const Value& copy,
                                   Value::Kind kind,
                                   const std::string& debug_string) {
    return ValueTestCase{std::move(value), copy,  kind,  copy, debug_string,
                         debug_string,     false, false, true};
  }
};

std::ostream& operator<<(std::ostream& os, const ValueTestCase& test_case) {
  return os << test_case.value;
}

class ValueTest : public ::testing::TestWithParam<ValueTestCase> {
 public:
  static absl::optional<Value> CreateRef(const Value& value) {
    switch (value.kind()) {
      case Value::Kind::kNull:
      case Value::Kind::kBool:
      case Value::Kind::kInt:
      case Value::Kind::kUInt:
      case Value::Kind::kDouble:
        // Inline values cannot be referenced.
        break;

      case Value::Kind::kEnum:
      case Value::Kind::kDuration:
      case Value::Kind::kTime:
      case Value::Kind::kError:
      case Value::Kind::kUnknown:
      case Value::Kind::kType:
        // These value types are always copied.
        break;

      case Value::Kind::kString:
        return Value::ForString(value.string_value());
      case Value::Kind::kBytes:
        return Value::ForBytes(value.bytes_value());
      case Value::Kind::kMap:
        return Value::ForMap(&value.map_value());
      case Value::Kind::kList:
        return Value::ForList(&value.list_value());
      case Value::Kind::kObject:
        return Value::ForObject(&value.object_value());
      case Value::Kind::DO_NOT_USE:  // Force a compiler error if this enum
                                     // is not complete.
        assert(false);
        break;
    }
    return absl::nullopt;
  }

  template <Value::Kind K, typename T>
  void TestKind();

  template <Value::Kind K, typename T, typename C>
  void TestCustomKind();
};

TEST_P(ValueTest, Kind) { EXPECT_EQ(GetParam().value.kind(), GetParam().kind); }

TEST_P(ValueTest, GetType) {
  EXPECT_EQ(GetParam().value.GetType(), GetParam().type);
}

TEST_P(ValueTest, TypeString) {
  EXPECT_EQ(GetParam().value.GetType().ToString(),
            GetParam().type_debug_string);
}

TEST_P(ValueTest, ToString) {
  std::ostringstream os;
  os << GetParam().value;
  EXPECT_EQ(os.str(), GetParam().value.ToString());
  EXPECT_EQ(GetParam().value.ToString(), GetParam().debug_string);
}

TEST_P(ValueTest, Inlined) {
  EXPECT_EQ(GetParam().value.is_inline(), GetParam().is_inline);
}

TEST_P(ValueTest, IsValue) {
  EXPECT_EQ(GetParam().value.is_value(), GetParam().is_value);
}

TEST_P(ValueTest, IsObjet) {
  bool is_object = GetParam().type.kind() == Value::Kind::kType &&
                   GetParam().type.type_value().is_object();
  EXPECT_EQ(is_object, GetParam().value.is_object());
}

TEST_P(ValueTest, OwnsValue) {
  EXPECT_EQ(GetParam().value.owns_value(), GetParam().owns_value);
  auto ref = CreateRef(GetParam().value);
  if (ref.has_value()) {
    EXPECT_FALSE(ref.value().owns_value());
  }
}

TEST_P(ValueTest, For) {
  EXPECT_EQ(GetParam().value.owns_value(), GetParam().owns_value);
  auto ref = CreateRef(GetParam().value);
  if (ref.has_value()) {
    EXPECT_FALSE(ref.value().owns_value());
  }
}

template <Value::Kind K, typename T>
void ValueTest::TestKind() {
  using Kind = Value::Kind;
  Value value = GetParam().value;
  Kind kind = GetParam().kind;
  EXPECT_EQ(static_cast<bool>(value.get_if<K>()), kind == K);
  EXPECT_EQ(static_cast<bool>(value.get_if<T>()), kind == K);
  if (kind == K) {
    EXPECT_EQ(value, Value::From<K>(value.get<K>()));
    EXPECT_EQ(value, Value::From<K>(value.get<T>()));
  }
}

TEST_P(ValueTest, Null) { TestKind<Value::Kind::kNull, std::nullptr_t>(); }

TEST_P(ValueTest, Bool) { TestKind<Value::Kind::kBool, bool>(); }

TEST_P(ValueTest, Int) { TestKind<Value::Kind::kInt, int64_t>(); }

TEST_P(ValueTest, UInt) { TestKind<Value::Kind::kUInt, uint64_t>(); }

TEST_P(ValueTest, Double) { TestKind<Value::Kind::kDouble, double>(); }

TEST_P(ValueTest, Time) { TestKind<Value::Kind::kTime, absl::Time>(); }

TEST_P(ValueTest, Type) { TestKind<Value::Kind::kType, Type>(); }

TEST_P(ValueTest, Error) { TestKind<Value::Kind::kError, Error>(); }

TEST_P(ValueTest, Unknown) { TestKind<Value::Kind::kUnknown, Unknown>(); }

TEST_P(ValueTest, String) {
  using Kind = Value::Kind;
  Value value = GetParam().value;
  Kind kind = GetParam().kind;
  EXPECT_EQ(value.get_if<Kind::kString>(),
            kind == Kind::kString ? absl::make_optional(value.string_value())
                                  : absl::nullopt);
  if (kind == Kind::kString) {
    EXPECT_EQ(value, Value::From<Kind::kString>(value.get<Kind::kString>()));
    EXPECT_EQ(value, Value::For<Kind::kString>(value.get<Kind::kString>()));
  }
}

TEST_P(ValueTest, Bytes) {
  using Kind = Value::Kind;
  Value value = GetParam().value;
  Kind kind = GetParam().kind;
  EXPECT_EQ(value.get_if<Kind::kBytes>(),
            kind == Kind::kBytes ? absl::make_optional(value.bytes_value())
                                 : absl::nullopt);
  if (kind == Kind::kBytes) {
    EXPECT_EQ(value, Value::From<Kind::kBytes>(value.get<Kind::kBytes>()));
    EXPECT_EQ(value, Value::For<Kind::kBytes>(value.get<Kind::kBytes>()));
  }
}

TEST_P(ValueTest, TypeRoundTrip) {
  if (GetParam().type.kind() != Value::Kind::kType) {
    return;
  }
  Type expected = GetParam().type.type_value();
  Type actual(expected.full_name());
  EXPECT_EQ(expected, actual) << expected.full_name();
  EXPECT_EQ(expected.full_name(), actual.full_name());
  EXPECT_EQ(expected.is_object(), actual.is_object());
  EXPECT_EQ(expected.is_unrecognized(), actual.is_unrecognized());
  EXPECT_EQ(expected.is_enum(), actual.is_enum());
  EXPECT_EQ(expected.is_basic(), actual.is_basic());
}

template <Value::Kind K, typename T, typename C>
void ValueTest::TestCustomKind() {
  using Kind = Value::Kind;
  Value value = GetParam().value;
  Kind kind = GetParam().kind;
  EXPECT_EQ(value.get_if<K>() != nullptr, kind == K);
  EXPECT_EQ(value.get_if<T>() != nullptr, kind == K);
  EXPECT_EQ(value.get_if<C>() != nullptr, kind == K);
  if (kind == K) {
    EXPECT_EQ(value.get_if<K>(), &value.get<K>());
    EXPECT_EQ(value.get_if<T>(), &value.get<T>());
    EXPECT_EQ(value.get_if<C>(), &value.get<C>());
  }
}

TEST_P(ValueTest, Object) {
  TestCustomKind<Value::Kind::kObject, Object, DummyObject>();
}

TEST_P(ValueTest, Map) { TestCustomKind<Value::Kind::kMap, Map, DummyMap>(); }

TEST_P(ValueTest, List) {
  TestCustomKind<Value::Kind::kList, List, DummyList>();
}

TEST_P(ValueTest, Equal) {
  EXPECT_EQ(GetParam().value, GetParam().value);
  EXPECT_FALSE(GetParam().value != GetParam().value);
  EXPECT_EQ(GetParam().value, GetParam().value_copy);
  EXPECT_FALSE(GetParam().value != GetParam().value_copy);
  auto ref = CreateRef(GetParam().value);
  if (ref) {
    EXPECT_EQ(*ref, GetParam().value);
    EXPECT_FALSE(*ref != GetParam().value);
  }
}

TEST_P(ValueTest, HashCode) {
  EXPECT_EQ(GetParam().value.hash_code(), std::hash<Value>()(GetParam().value));
  EXPECT_EQ(GetParam().value.hash_code(), GetParam().value_copy.hash_code());
  auto ref = CreateRef(GetParam().value);
  if (ref) {
    EXPECT_EQ(ref->hash_code(), GetParam().value.hash_code());
  }
}

INSTANTIATE_TEST_SUITE_P(
    Inlined, ValueTest,
    ::testing::Values(
        ValueTestCase::ForInline(Value::NullValue(), BasicTypeValue::kNull,
                                 "null", "null_type"),
        ValueTestCase::ForInline(Value::FromBool(true), BasicTypeValue::kBool,
                                 "true", "bool"),
        ValueTestCase::ForInline(Value::FromInt(1), BasicTypeValue::kInt, "1",
                                 "int"),
        ValueTestCase::ForInline(Value::FromUInt(1), BasicTypeValue::kUint,
                                 "1u", "uint"),
        ValueTestCase::ForInline(Value::FromDouble(1), BasicTypeValue::kDouble,
                                 "1.0", "double"),
        ValueTestCase::ForInline(Value::FromType(BasicTypeValue::kInt),
                                 BasicTypeValue::kType, "int", "type"),
        ValueTestCase::ForInline(
            Value::FromType(ObjectType::For<google::protobuf::Timestamp>()),
            BasicTypeValue::kType, "google.protobuf.Timestamp", "type"),
        ValueTestCase::ForInline(
            Value::FromType(EnumType(google::protobuf::NullValue_descriptor())),
            BasicTypeValue::kType, "google.protobuf.NullValue", "type"),
        ValueTestCase::ForInline(
            Value::FromEnum(NamedEnumValue(
                google::protobuf::NullValue_descriptor()->value(0))),
            EnumType(google::protobuf::NullValue_descriptor()),
            "google.protobuf.NULL_VALUE", "google.protobuf.NullValue"),
        ValueTestCase::ForInline(Value::FromUnknown(Id(1)), "Unknown{Id(1)}",
                                 "Unknown{Id(1)}")));

INSTANTIATE_TEST_SUITE_P(
    NonInlined, ValueTest,
    ::testing::Values(
        ValueTestCase::ForNonInline(Value::ForString("hi"),
                                    Value::ForString("hi"),
                                    BasicTypeValue::kString, "\"hi\"", "string",
                                    false),
        ValueTestCase::ForNonInline(
            Value::FromBytes(absl::string_view("h\000i", 3)),
            Value::FromBytes(absl::string_view("h\000i", 3)),
            BasicTypeValue::kBytes, "b\"h\\000i\"", "bytes"),
        ValueTestCase::ForNonInline(Value::MakeList<DummyList>(),
                                    Value::MakeList<DummyList>(),
                                    BasicTypeValue::kList, "[\"elem\"]",
                                    "list"),
        ValueTestCase::ForNonInline(Value::MakeMap<DummyMap>(),
                                    Value::MakeMap<DummyMap>(),
                                    BasicTypeValue::kMap,
                                    "{\"key\": \"value\"}", "map"),
        ValueTestCase::ForNonInline(Value::MakeObject<DummyObject>(),
                                    Value::MakeObject<DummyObject>(),
                                    Value::Kind::kObject, "google.type.Money",
                                    "{}"),
        ValueTestCase::ForNonInline(
            Value::FromDuration(absl::Hours(1) + absl::Nanoseconds(1)),
            Value::FromDuration(absl::Hours(1) + absl::Nanoseconds(1)),
            Value::Kind::kDuration, "google.protobuf.Duration",
            "(\"1h0.000000001s\")"),
        ValueTestCase::ForNonInline(Value::FromTime(absl::FromUnixNanos(1)),
                                    Value::FromTime(absl::FromUnixNanos(1)),
                                    Value::Kind::kTime,
                                    "google.protobuf.Timestamp",
                                    "(\"1970-01-01T00:00:00.000000001Z\")"),
        ValueTestCase::ForNonInline(
            Value::FromEnum(EnumValue(
                EnumType(google::protobuf::NullValue_descriptor()), -1)),
            Value::FromEnum(EnumValue(
                EnumType(google::protobuf::NullValue_descriptor()), -1)),
            EnumType(google::protobuf::NullValue_descriptor()),
            "google.protobuf.NullValue(-1)", "google.protobuf.NullValue"),
        ValueTestCase::ForNonInline(Value::FromType("bad type"),
                                    Value::FromType("bad type"),
                                    BasicTypeValue::kType, "type(\"bad type\")",
                                    "type")));

INSTANTIATE_TEST_SUITE_P(
    NonValue, ValueTest,
    ::testing::Values(
        ValueTestCase::ForNonValue(
            Value::FromError(internal::NotFoundError("hi")),
            Value::FromError(internal::NotFoundError("hi")),
            Value::Kind::kError, "Error{NOT_FOUND}"),
        ValueTestCase::ForNonValue(
            Value::FromError(Error({internal::NotFoundError("hi"),
                                    internal::OutOfRangeError("bye")})),
            Value::FromError(Error({internal::NotFoundError("hi"),
                                    internal::OutOfRangeError("bye")})),
            Value::Kind::kError, "Error{NOT_FOUND, OUT_OF_RANGE}"),
        ValueTestCase::ForNonValue(Value::FromUnknown(Unknown({Id(1), Id(2)})),
                                   Value::FromUnknown(Unknown({Id(1), Id(2)})),
                                   Value::Kind::kUnknown,
                                   "Unknown{Id(1), Id(2)}")));

TEST(ValueTest, NotEqual) {
  EXPECT_NE(Value::ForString("hi"), Value::ForBytes("hi"));
  EXPECT_NE(Value::ForString("hi").hash_code(),
            Value::ForBytes("hi").hash_code());
  EXPECT_NE(Value::FromInt(1), Value::FromUInt(1));
  EXPECT_NE(Value::FromInt(1).hash_code(), Value::FromUInt(1).hash_code());
}

template <typename T, typename U>
void TestGetIfNum(Value value, bool matches) {
  ExpectSameType<absl::optional<T>, decltype(value.get_if<T>())>();
  ExpectSameType<T, decltype(value.get<T>())>();
  if (matches) {
    EXPECT_EQ(value.get<U>(), *value.get_if<T>());
    EXPECT_EQ(value.get<U>(), value.get<T>());
  } else {
    EXPECT_EQ(absl::nullopt, value.get_if<T>());
  }
}

TEST(ValueTest, GetIfInt) {
  Value max_int = Value::FromInt(std::numeric_limits<int64_t>::max());
  EXPECT_EQ(std::numeric_limits<int64_t>::max(), max_int.get<int64_t>());
  EXPECT_EQ(std::numeric_limits<int64_t>::max(), *max_int.get_if<int64_t>());

  TestGetIfNum<int32_t, int64_t>(max_int, false);
  TestGetIfNum<int16_t, int64_t>(max_int, false);
  TestGetIfNum<int8_t, int64_t>(max_int, false);

  Value one = Value::FromInt(1);
  EXPECT_EQ(1, one.get<int64_t>());
  EXPECT_EQ(1, *one.get_if<int64_t>());
  EXPECT_EQ(nullptr, one.get_if<uint64_t>());

  TestGetIfNum<int32_t, int64_t>(one, true);
  TestGetIfNum<int16_t, int64_t>(one, true);
  TestGetIfNum<int8_t, int64_t>(one, true);
}

TEST(ValueTest, GetIfUInt) {
  Value max_uint = Value::FromUInt(std::numeric_limits<uint64_t>::max());
  EXPECT_EQ(std::numeric_limits<uint64_t>::max(), max_uint.get<uint64_t>());
  EXPECT_EQ(std::numeric_limits<uint64_t>::max(), *max_uint.get_if<uint64_t>());

  TestGetIfNum<uint32_t, uint64_t>(max_uint, false);
  TestGetIfNum<uint16_t, uint64_t>(max_uint, false);
  TestGetIfNum<uint8_t, uint64_t>(max_uint, false);

  Value one = Value::FromUInt(1);
  EXPECT_EQ(1, one.get<uint64_t>());
  EXPECT_EQ(1, *one.get_if<uint64_t>());
  EXPECT_EQ(nullptr, one.get_if<int64_t>());

  TestGetIfNum<uint32_t, uint64_t>(one, true);
  TestGetIfNum<uint16_t, uint64_t>(one, true);
  TestGetIfNum<uint8_t, uint64_t>(one, true);
}

TEST(ValueTest, GetIfDouble) {
  Value max_double = Value::FromDouble(std::numeric_limits<double>::max());
  EXPECT_EQ(std::numeric_limits<double>::max(), max_double.get<double>());
  EXPECT_EQ(std::numeric_limits<double>::max(), *max_double.get_if<double>());

  TestGetIfNum<long double, double>(max_double, true);
  TestGetIfNum<float, double>(max_double, false);

  Value one = Value::FromDouble(1);
  EXPECT_EQ(1, one.get<double>());
  EXPECT_EQ(1, *one.get_if<double>());

  TestGetIfNum<long double, double>(one, true);
  TestGetIfNum<float, double>(one, true);

  Value inf = Value::FromDouble(std::numeric_limits<double>::infinity());
  EXPECT_TRUE(std::isinf(inf.get<double>()));
  EXPECT_TRUE(std::isinf(*inf.get_if<double>()));
  EXPECT_TRUE(std::isinf(inf.get<float>()));
  EXPECT_TRUE(std::isinf(*inf.get_if<float>()));
  EXPECT_TRUE(std::isinf(inf.get<long double>()));
  EXPECT_TRUE(std::isinf(*inf.get_if<long double>()));
}

TEST(ErrorTest, FromStatus) {
  Error value(internal::OutOfRangeError("hi"));
  EXPECT_EQ(value.errors().size(), 1);
  EXPECT_THAT(value.errors(), ::testing::Contains(testutil::EqualsProto(
                                  internal::OutOfRangeError("hi"))));
}

TEST(ErrorTest, FromInitList) {
  Error value(
      {internal::OutOfRangeError("hi"), internal::OutOfRangeError("bye")});
  EXPECT_EQ(value.errors().size(), 2);
  EXPECT_THAT(value.errors(), ::testing::Contains(testutil::EqualsProto(
                                  internal::OutOfRangeError("hi"))));
  EXPECT_THAT(value.errors(), ::testing::Contains(testutil::EqualsProto(
                                  internal::OutOfRangeError("bye"))));
}

TEST(ErrorTest, FromRepeated) {
  Error value(
      {internal::OutOfRangeError("hi"), internal::OutOfRangeError("bye")});
  EXPECT_EQ(value.errors().size(), 2);
  EXPECT_THAT(value.errors(), ::testing::Contains(testutil::EqualsProto(
                                  internal::OutOfRangeError("hi"))));
  EXPECT_THAT(value.errors(), ::testing::Contains(testutil::EqualsProto(
                                  internal::OutOfRangeError("bye"))));
}

TEST(ErrorTest, OrderAgnostic) {
  Error v1({internal::OutOfRangeError("hi"), internal::OutOfRangeError("bye")});
  Error v2({internal::OutOfRangeError("bye"), internal::OutOfRangeError("hi")});
  Error v3({internal::OutOfRangeError("hi"), internal::OutOfRangeError("hi")});
  EXPECT_EQ(v1, v2);
  EXPECT_EQ(v1.hash_code(), v2.hash_code());
  EXPECT_NE(v1, v3);
  // Technically could be equal (but likely shouldn't be);
  EXPECT_NE(v1.hash_code(), v3.hash_code());
}

TEST(UnknownTest, FromId) {
  Unknown value(Id(1));
  EXPECT_EQ(value.ids(), std::set<Id>({Id(1)}));
}

TEST(UnknownTest, FromInitList) {
  Unknown value({Id(1), Id(2)});
  EXPECT_EQ(value.ids(), std::set<Id>({Id(1), Id(2)}));
}

TEST(UnknownTest, OrderAgnostic) {
  Unknown v1({Id(1), Id(2), Id(3)});
  Unknown v2({Id(3), Id(1), Id(2)});
  Unknown v3({Id(3), Id(1)});
  EXPECT_EQ(v1, v2);
  EXPECT_EQ(v1.hash_code(), v2.hash_code());
  EXPECT_NE(v1, v3);
  // Technically could be equal (but likely shouldn't be);
  EXPECT_NE(v1.hash_code(), v3.hash_code());
}

TEST(TypeTest, BasicType) {
  Type value((BasicType(BasicTypeValue::kString)));
  EXPECT_TRUE(value.is_basic());
  EXPECT_FALSE(value.is_object());
  EXPECT_FALSE(value.is_enum());
  EXPECT_EQ(value.basic_type().value(), BasicTypeValue::kString);
  EXPECT_EQ("string", value.full_name());
}

TEST(TypeTest, ObjectType) {
  ObjectType type(google::type::Money::descriptor());
  Type value(type);
  EXPECT_FALSE(value.is_basic());
  EXPECT_TRUE(value.is_object());
  EXPECT_FALSE(value.is_enum());
  EXPECT_EQ(value.object_type(), type);
  EXPECT_EQ(value.full_name(), "google.type.Money");
  EXPECT_EQ(value.object_type().value()->full_name(), "google.type.Money");
}

TEST(TypeTest, EnumType) {
  EnumType type(google::protobuf::NullValue_descriptor());
  Type value(type);
  EXPECT_FALSE(value.is_basic());
  EXPECT_FALSE(value.is_object());
  EXPECT_TRUE(value.is_enum());
  EXPECT_EQ(value.enum_type(), type);
  EXPECT_EQ(value.full_name(), "google.protobuf.NullValue");
  EXPECT_EQ(value.enum_type().value()->full_name(),
            "google.protobuf.NullValue");
}

}  // namespace
}  // namespace common

namespace internal {

using testutil::ExpectSameType;

class ValueVisitTest : public ::testing::Test {
 public:
  template <typename T>
  using ValueAdapterType =
      decltype(MaybeAdapt(BaseValue::ValueAdapter(), inst_of<T>()));

  template <typename H, typename T>
  using GetPtrVisitorType =
      decltype(BaseValue::GetPtrVisitor<DefaultVisitor<T*>, T>()(inst_of<H>()));

  template <typename H, typename R, typename T>
  using GetVisitorType =
      decltype(BaseValue::GetVisitor<DefaultVisitor<R>, R, T>()(inst_of<H>()));

  using OwnedStr = BaseValue::OwnedStr;
  using UnownedStr = BaseValue::UnownedStr;
  using ParentOwnedStr = BaseValue::ParentOwnedStr;

  template <typename H>
  void TestGetPtrVisitor() {
    using T = absl::remove_reference_t<decltype(*inst_of<H&>())>;
    ExpectSameType<const T*, GetPtrVisitorType<H, T>>();
    // Return by const ref.
    ExpectSameType<const T&, GetVisitorType<H, const T&, T>>();
  }

  template <typename H, typename U>
  void TestGetVisitor() {
    using T = absl::remove_reference_t<decltype(*inst_of<H&>())>;
    // Return by optional.
    ExpectSameType<absl::optional<U>,
                   GetVisitorType<H, absl::optional<U>, T>>();
    // Return by value.
    ExpectSameType<U, GetVisitorType<H, U, T>>();
  }

  template <typename H>
  void TestValueAdapter() {
    using T = absl::remove_reference_t<decltype(*inst_of<H&>())>;
    ExpectSameType<T&, ValueAdapterType<H&>>();
    ExpectSameType<const T&, ValueAdapterType<const H&>>();
  }
};

TEST_F(ValueVisitTest, Types) {
  TestValueAdapter<CopyHolder<std::nullptr_t>>();

  TestValueAdapter<CopyHolder<bool>>();
  TestGetPtrVisitor<CopyHolder<bool>>();

  TestValueAdapter<CopyHolder<int64_t>>();
  TestGetPtrVisitor<CopyHolder<int64_t>>();

  TestValueAdapter<CopyHolder<uint64_t>>();
  TestGetPtrVisitor<CopyHolder<uint64_t>>();

  TestValueAdapter<CopyHolder<double>>();
  TestGetPtrVisitor<CopyHolder<double>>();

  TestValueAdapter<CopyHolder<common::NamedEnumValue>>();
  TestGetPtrVisitor<CopyHolder<common::NamedEnumValue>>();
  TestGetVisitor<CopyHolder<common::NamedEnumValue>, common::EnumValue>();

  TestValueAdapter<CopyHolder<common::BasicType>>();
  TestGetPtrVisitor<CopyHolder<common::BasicType>>();
  TestGetVisitor<CopyHolder<common::BasicType>, common::Type>();

  TestValueAdapter<CopyHolder<common::ObjectType>>();
  TestGetPtrVisitor<CopyHolder<common::ObjectType>>();
  TestGetVisitor<CopyHolder<common::ObjectType>, common::Type>();

  TestValueAdapter<CopyHolder<common::EnumType>>();
  TestGetPtrVisitor<CopyHolder<common::EnumType>>();
  TestGetVisitor<CopyHolder<common::EnumType>, common::Type>();

  ExpectSameType<absl::string_view, ValueAdapterType<OwnedStr&>>();
  ExpectSameType<absl::string_view, ValueAdapterType<const OwnedStr&>>();
  ExpectSameType<absl::string_view, ValueAdapterType<UnownedStr&>>();
  ExpectSameType<absl::string_view, ValueAdapterType<const UnownedStr&>>();
  ExpectSameType<absl::string_view, ValueAdapterType<ParentOwnedStr&>>();
  ExpectSameType<absl::string_view, ValueAdapterType<const ParentOwnedStr&>>();

  TestValueAdapter<UnownedPtrHolder<const common::Map>>();
  TestValueAdapter<RefPtrHolder<common::Map>>();
  TestGetPtrVisitor<UnownedPtrHolder<const common::Map>>();
  TestGetPtrVisitor<RefPtrHolder<const common::Map>>();

  TestValueAdapter<UnownedPtrHolder<const common::List>>();
  TestValueAdapter<RefPtrHolder<common::List>>();
  TestGetPtrVisitor<UnownedPtrHolder<const common::List>>();
  TestGetPtrVisitor<RefPtrHolder<const common::List>>();

  TestValueAdapter<UnownedPtrHolder<const common::Object>>();
  TestValueAdapter<RefPtrHolder<common::Object>>();
  TestGetPtrVisitor<UnownedPtrHolder<const common::Object>>();
  TestGetPtrVisitor<RefPtrHolder<const common::Object>>();

  TestValueAdapter<RefCopyHolder<absl::Duration>>();
  TestValueAdapter<RefCopyHolder<absl::Time>>();
  TestValueAdapter<RefCopyHolder<common::UnnamedEnumValue>>();
  TestValueAdapter<RefCopyHolder<common::UnrecognizedType>>();
  TestValueAdapter<RefCopyHolder<common::Error>>();
  TestValueAdapter<RefCopyHolder<common::Unknown>>();
}

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google
