#include "testutil/test_data_util.h"

#include "google/protobuf/any.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "google/rpc/code.pb.h"
#include "absl/strings/str_cat.h"
#include "common/value.h"
#include "internal/cel_printer.h"
#include "internal/proto_util.h"
#include "protoutil/converters.h"
#include "protoutil/type_registry.h"
#include "v1beta1/converters.h"

namespace google {
namespace api {
namespace expr {

using internal::CelPrinter;
using protoutil::TypeRegistry;
using testdata::TestProtoValue;
using testdata::TestValue;

namespace v1beta1 {

void InitValueDifferencer(google::protobuf::util::MessageDifferencer* differencer) {
  static google::protobuf::util::DefaultFieldComparator* field_comparator = []() {
    auto* result = new google::protobuf::util::DefaultFieldComparator();
    result->set_float_comparison(google::protobuf::util::DefaultFieldComparator::EXACT);
    result->set_treat_nan_as_equal(true);
    return result;
  }();

  auto map_entry_field = v1beta1::Value::descriptor()
                             ->FindFieldByName("map_value")
                             ->message_type()
                             ->FindFieldByName("entries");
  auto key_field = map_entry_field->message_type()->FindFieldByName("key");
  differencer->TreatAsMap(map_entry_field, key_field);
  differencer->TreatAsSet(
      v1beta1::ErrorSet::descriptor()->FindFieldByName("errors"));
  differencer->TreatAsSet(
      google::rpc::Status::descriptor()->FindFieldByName("details"));
  differencer->TreatAsSet(
      v1beta1::UnknownSet::descriptor()->FindFieldByName("exprs"));
  differencer->set_field_comparator(field_comparator);
}

}  // namespace v1beta1

namespace testutil {

namespace {

const TypeRegistry* GetRegistry() {
  static const TypeRegistry* reg = []() {
    auto* reg = new TypeRegistry();
    protoutil::RegisterConvertersWith(reg);
    return reg;
  }();
  return reg;
}

TestProtoValue* AddProto(TestValue* value, absl::string_view field) {
  TestProtoValue* result = value->add_proto();
  result->set_value_field_name(std::string(field));
  return result;
}

TestProtoValue* FindProto(TestValue* value, absl::string_view field) {
  for (auto& proto : *value->mutable_proto()) {
    if (proto.value_field_name() == field) {
      return &proto;
    }
  }
  return nullptr;
}

const TestProtoValue* FindProto(const TestValue* value,
                                absl::string_view field) {
  for (auto& proto : value->proto()) {
    if (proto.value_field_name() == field) {
      return &proto;
    }
  }
  return nullptr;
}

void InitAll(TestValue* value, absl::string_view prefix) {
  for (int i = 0; i < TestProtoValue::descriptor()->field_count(); ++i) {
    absl::string_view field_name =
        TestProtoValue::descriptor()->field(i)->name();
    if (absl::StartsWith(field_name, prefix)) {
      AddProto(value, field_name);
    }
  }
}

void RemoveProto(TestValue* value, absl::string_view field) {
  for (auto itr = value->mutable_proto()->begin();
       itr != value->mutable_proto()->end();) {
    if (itr->value_field_name() == field) {
      itr = value->mutable_proto()->erase(itr);
    } else {
      ++itr;
    }
  }
}

template <typename T, typename F>
bool rep_as(F value) {
  if (value < std::numeric_limits<T>::min() ||
      value > std::numeric_limits<T>::max()) {
    return false;
  }

  return static_cast<F>(static_cast<T>(value)) == value;
}

template <typename T>
std::string MakeName(absl::string_view type, T&& value,
                     absl::string_view name = "") {
  if (name.empty()) {
    return absl::StrCat(type, "(", value, ")");
  }
  return absl::StrCat(type, "(", name, ")");
}

template <typename T>
TestValue MakeValue(absl::string_view type, T&& value,
                    absl::string_view name = "") {
  TestValue result;
  result.set_name(MakeName(type, value, name));
  result.add_expr(CelPrinter()(value));
  return result;
}

}  // namespace

std::vector<std::pair<TestValue, TestValue>> AllPairs(
    const testdata::TestValues& value_cases) {
  std::vector<std::pair<TestValue, TestValue>> result;
  for (int i = 0; i < value_cases.values().size(); ++i) {
    for (int j = i + 1; j < value_cases.values().size(); ++j) {
      result.emplace_back(value_cases.values(i), value_cases.values(j));
    }
  }

  return result;
}

TestValue NewValue(std::nullptr_t value) {
  TestValue result;
  result.set_name("null");

  result.add_expr("null");

  // v1beta1 values.
  result.add_v1beta1()->mutable_value()->set_null_value(
      google::protobuf::NullValue::NULL_VALUE);

  // proto values
  // An unset well-known types.
  AddProto(&result, "single_any");
  AddProto(&result, "single_duration");
  AddProto(&result, "single_timestamp");
  AddProto(&result, "single_null");
  InitAll(&result, "wrapped_");

  // json_values
  google::protobuf::Value json_null;
  json_null.set_null_value(google::protobuf::NullValue::NULL_VALUE);
  *AddProto(&result, "single_value")->mutable_single_value() = json_null;
  AddProto(&result, "single_any")->mutable_single_any()->PackFrom(json_null);

  return result;
}

TestValue NewValue(bool value) {
  TestValue result = MakeValue("bool", value, CelPrinter()(value));

  // v1beta1 values.
  result.add_v1beta1()->mutable_value()->set_bool_value(value);

  // proto values.
  AddProto(&result, "single_bool")->set_single_bool(value);

  // A json values.
  google::protobuf::Value json_bool;
  json_bool.set_bool_value(value);
  *AddProto(&result, "single_value")->mutable_single_value() = json_bool;
  AddProto(&result, "single_any")->mutable_single_any()->PackFrom(json_bool);

  // wrapped values.
  google::protobuf::BoolValue wrapped_bool;
  wrapped_bool.set_value(value);
  *AddProto(&result, "wrapped_bool")->mutable_wrapped_bool() = wrapped_bool;
  AddProto(&result, "single_any")->mutable_single_any()->PackFrom(wrapped_bool);
  return result;
}

TestValue NewValue(double value, absl::string_view name) {
  TestValue result = MakeValue("double", value, name);

  // v1beta1 values.
  result.add_v1beta1()->mutable_value()->set_double_value(value);

  // proto values.
  AddProto(&result, "single_double")->set_single_double(value);
  if (rep_as<float>(value)) {
    AddProto(&result, "single_float")->set_single_float(value);
  }

  // josn values.
  google::protobuf::Value json_double;
  json_double.set_number_value(value);
  // A json bool
  *AddProto(&result, "single_value")->mutable_single_value() = json_double;
  // A json bool in an any.
  AddProto(&result, "single_any")->mutable_single_any()->PackFrom(json_double);

  // wrapped values.
  google::protobuf::DoubleValue wrapped_double;
  wrapped_double.set_value(value);
  *AddProto(&result, "wrapped_double")->mutable_wrapped_double() =
      wrapped_double;
  AddProto(&result, "single_any")
      ->mutable_single_any()
      ->PackFrom(wrapped_double);
  if (rep_as<float>(value)) {
    google::protobuf::FloatValue wrapped_float;
    wrapped_float.set_value(value);
    *AddProto(&result, "wrapped_float")->mutable_wrapped_float() =
        wrapped_float;
    AddProto(&result, "single_any")
        ->mutable_single_any()
        ->PackFrom(wrapped_double);
  }
  return result;
}

TestValue NewValue(int64_t value, absl::string_view name) {
  TestValue result = MakeValue("int", value, name);

  // v1beta1 value.
  result.add_v1beta1()->mutable_value()->set_int64_value(value);

  // proto values.
  AddProto(&result, "single_int64")->set_single_int64(value);
  AddProto(&result, "single_sint64")->set_single_sint64(value);
  AddProto(&result, "single_sfixed64")->set_single_sfixed64(value);
  if (rep_as<int32_t>(value)) {
    AddProto(&result, "single_int32")->set_single_int32(value);
    AddProto(&result, "single_sint32")->set_single_sint32(value);
    AddProto(&result, "single_sfixed32")->set_single_sfixed32(value);
    AddProto(&result, "wrapped_int32")
        ->mutable_wrapped_int32()
        ->set_value(value);
  }

  // wrapped values
  AddProto(&result, "wrapped_int64")->mutable_wrapped_int64()->set_value(value);
  if (rep_as<int32_t>(value)) {
    AddProto(&result, "wrapped_int32")
        ->mutable_wrapped_int32()
        ->set_value(value);
  }

  return result;
}

TestValue NewValue(uint64_t value, absl::string_view name) {
  TestValue result = MakeValue("uint", value, name);
  result.add_v1beta1()->mutable_value()->set_uint64_value(value);

  // proto values.
  AddProto(&result, "single_uint64")->set_single_uint64(value);
  AddProto(&result, "single_fixed64")->set_single_fixed64(value);
  if (rep_as<int32_t>(value)) {
    AddProto(&result, "single_uint32")->set_single_uint32(value);
    AddProto(&result, "single_fixed32")->set_single_fixed32(value);
  }

  // wrapped values.
  google::protobuf::UInt64Value wrapped_uint64;
  wrapped_uint64.set_value(value);
  *AddProto(&result, "wrapped_uint64")->mutable_wrapped_uint64() =
      wrapped_uint64;
  AddProto(&result, "single_any")
      ->mutable_single_any()
      ->PackFrom(wrapped_uint64);
  if (rep_as<int32_t>(value)) {
    google::protobuf::UInt32Value wrapped_uint32;
    wrapped_uint32.set_value(value);
    *AddProto(&result, "wrapped_uint32")->mutable_wrapped_uint32() =
        wrapped_uint32;
    AddProto(&result, "single_any")
        ->mutable_single_any()
        ->PackFrom(wrapped_uint32);
  }

  return result;
}

TestValue NewValue(absl::string_view value, absl::string_view name) {
  TestValue result = MakeValue("string", value, name);

  // v1beta1 values.
  result.add_v1beta1()->mutable_value()->set_string_value(std::string(value));

  // proto values
  AddProto(&result, "single_string")->set_single_string(std::string(value));

  // json values.
  google::protobuf::Value json_string;
  json_string.set_string_value(std::string(value));
  *AddProto(&result, "single_value")->mutable_single_value() = json_string;
  AddProto(&result, "single_any")->mutable_single_any()->PackFrom(json_string);

  // wrapped values.
  google::protobuf::StringValue wrapped_string;
  wrapped_string.set_value(std::string(value));
  *AddProto(&result, "wrapped_string")->mutable_wrapped_string() =
      wrapped_string;
  AddProto(&result, "single_any")
      ->mutable_single_any()
      ->PackFrom(wrapped_string);

  return result;
}

TestValue NewValue(const char* value, absl::string_view name) {
  return NewValue(absl::string_view(value), name);
}

TestValue NewBytesValue(absl::string_view value, absl::string_view name) {
  TestValue result = MakeValue("bytes", value, name);
  // v1beta1 values.
  result.add_v1beta1()->mutable_value()->set_bytes_value(std::string(value));

  // proto values
  AddProto(&result, "single_bytes")->set_single_bytes(std::string(value));

  // wrapped values.
  google::protobuf::BytesValue wrapped_bytes;
  wrapped_bytes.set_value(value.data());
  *AddProto(&result, "wrapped_bytes")->mutable_wrapped_bytes() = wrapped_bytes;
  AddProto(&result, "single_any")
      ->mutable_single_any()
      ->PackFrom(wrapped_bytes);

  return result;
}

TestValue NewValue(const google::protobuf::Message& value, absl::string_view name) {
  TestValue result;
  result.set_name(MakeName(value.GetTypeName(), name, name));

  result.add_expr(GetRegistry()->ValueFor(&value).ToString());

  // v1beta1 values
  result.add_v1beta1()->mutable_value()->mutable_object_value()->PackFrom(
      value);

  // proto values.
  AddProto(&result, "single_any")->mutable_single_any()->PackFrom(value);

  return result;
}

TestValue NewValue(absl::Duration value, absl::string_view name) {
  google::protobuf::Duration duration;
  auto status = expr::internal::EncodeDuration(value, &duration);
  assert(status.ok());
  auto result = NewValue(duration, name);
  *AddProto(&result, "single_duration")->mutable_single_duration() = duration;
  return result;
}

TestValue NewValue(absl::Time value, absl::string_view name) {
  google::protobuf::Timestamp timestamp;
  auto status = expr::internal::EncodeTime(value, &timestamp);
  assert(status.ok());
  auto result = NewValue(timestamp, name);
  *AddProto(&result, "single_timestamp")->mutable_single_timestamp() =
      timestamp;
  return result;
}

testdata::TestValue NewValue(const common::Type& type) {
  TestValue result;
  result.add_expr(type.ToString());

  v1beta1::ValueTo(common::Value::FromType(type), result.add_v1beta1());
  return result;
}

namespace internal {

void AppendToList(TestValue* list_value) {}

#define SINGLE_TO_REPEATED(name)                                 \
  if (proto->value_field_name() == "repeated_" #name) {          \
    if (auto* value = FindProto(&test_value, "single_" #name)) { \
      proto->add_repeated_##name(value->single_##name());        \
      return true;                                               \
    }                                                            \
  }

#define SINGLE_TO_REPEATED_EQ(name)                              \
  if (proto->value_field_name() == "repeated_" #name) {          \
    if (auto* value = FindProto(&test_value, "single_" #name)) { \
      if (value->has_single_##name()) {                          \
        *proto->add_repeated_##name() = value->single_##name();  \
        return true;                                             \
      }                                                          \
    }                                                            \
  }

bool AppendToProtoList(const TestValue& test_value, TestProtoValue* proto) {
  if (proto->value_field_name() == "single_list") {
    if (auto* value = FindProto(&test_value, "single_value")) {
      *proto->mutable_single_list()->add_values() = value->single_value();
      return true;
    }
  }
  SINGLE_TO_REPEATED(bool);
  SINGLE_TO_REPEATED(bytes);
  SINGLE_TO_REPEATED(string);  // no transform
  SINGLE_TO_REPEATED(double);
  SINGLE_TO_REPEATED(float);
  SINGLE_TO_REPEATED(int32);   // no transform
  SINGLE_TO_REPEATED(int64);   // no transform
  SINGLE_TO_REPEATED(uint32);  // no transform
  SINGLE_TO_REPEATED(uint64);  // no transform
  SINGLE_TO_REPEATED(sint32);
  SINGLE_TO_REPEATED(sint64);
  SINGLE_TO_REPEATED(fixed32);
  SINGLE_TO_REPEATED(fixed64);
  SINGLE_TO_REPEATED(sfixed32);
  SINGLE_TO_REPEATED(sfixed64);
  SINGLE_TO_REPEATED(nested_enum);
  SINGLE_TO_REPEATED(null);
  SINGLE_TO_REPEATED_EQ(nested_message);
  SINGLE_TO_REPEATED_EQ(duration);
  SINGLE_TO_REPEATED_EQ(timestamp);
  SINGLE_TO_REPEATED_EQ(any);
  SINGLE_TO_REPEATED_EQ(struct);
  SINGLE_TO_REPEATED_EQ(value);
  SINGLE_TO_REPEATED_EQ(list);
  return false;
}

#undef SINGLE_TO_REPEATED
#undef SINGLE_TO_REPEATED_EQ

void AppendToList(const TestValue& test_value, TestValue* list_value) {
  for (auto& v1beta1 : *list_value->mutable_v1beta1()) {
    *v1beta1.mutable_value()->mutable_list_value()->add_values() =
        test_value.v1beta1(0).value();
  }

  for (auto& expr : *list_value->mutable_expr()) {
    absl::StrAppend(&expr, test_value.expr(0), ", ");
  }

  for (auto itr = list_value->mutable_proto()->begin();
       itr != list_value->mutable_proto()->end();) {
    if (!AppendToProtoList(test_value, &(*itr))) {
      itr = list_value->mutable_proto()->erase(itr);
    } else {
      ++itr;
    }
  }
}

#define SINGLES_TO_MAP(key_name, value_name)                           \
  if (proto->value_field_name() == "map_" #key_name "_" #value_name) { \
    auto* key_value = FindProto(&key, "single_" #key_name);            \
    auto* value_value = FindProto(&value, "single_" #value_name);      \
    if (key_value && value_value) {                                    \
      (*proto->mutable_map_##key_name##_##value_name())                \
          [key_value->single_##key_name()] =                           \
              value_value->single_##value_name();                      \
      return true;                                                     \
    }                                                                  \
  }

bool AddToProtoMap(const TestValue& key, const TestValue& value,
                   TestProtoValue* proto) {
  if (proto->value_field_name() == "single_struct") {
    auto* string_key = FindProto(&key, "single_string");
    auto* value_value = FindProto(&value, "single_value");
    if (string_key && value_value) {
      (*proto->mutable_single_struct()
            ->mutable_fields())[string_key->single_string()] =
          value_value->single_value();
      return true;
    }
  }

  SINGLES_TO_MAP(int64, string);   // no transform
  SINGLES_TO_MAP(uint64, string);  // no transform
  SINGLES_TO_MAP(bool, string);    // no transform
  SINGLES_TO_MAP(string, int64);   // no transform
  SINGLES_TO_MAP(string, uint64);  // no transform
  SINGLES_TO_MAP(string, bool);    // no transform
  SINGLES_TO_MAP(string, string);  // no transform
  return false;
}

#undef SINGLES_TO_MAP

void AddToMap(TestValue* map_value) {}

void AddToMap(const TestValue& key, const TestValue& value,
              TestValue* map_value) {
  for (auto& v1beta1 : *map_value->mutable_v1beta1()) {
    auto& entry = *v1beta1.mutable_value()->mutable_map_value()->add_entries();
    *entry.mutable_key() = key.v1beta1(0).value();
    *entry.mutable_value() = value.v1beta1(0).value();
  }

  for (auto& expr : *map_value->mutable_expr()) {
    absl::StrAppend(&expr, key.expr(0), ": ", value.expr(0), ", ");
  }

  for (auto itr = map_value->mutable_proto()->begin();
       itr != map_value->mutable_proto()->end();) {
    if (!AddToProtoMap(key, value, &(*itr))) {
      itr = map_value->mutable_proto()->erase(itr);
    } else {
      ++itr;
    }
  }
}

void StartList(testdata::TestValue* list_value) {
  list_value->add_v1beta1()->mutable_value()->mutable_list_value();
  list_value->add_expr("[");
  AddProto(list_value, "single_list")->mutable_single_list();
  InitAll(list_value, "repeated_");
}

void EndList(testdata::TestValue* list_value) {
  for (auto& expr : *list_value->mutable_expr()) {
    if (absl::EndsWith(expr, ", ")) {
      expr.erase(expr.size() - 2);
    }
    absl::StrAppend(&expr, "]");
  }
}

void StartMap(testdata::TestValue* map_value) {
  map_value->add_v1beta1()->mutable_value()->mutable_map_value();
  map_value->add_expr("{");
  AddProto(map_value, "single_struct")->mutable_single_struct();
  InitAll(map_value, "map_");
}

void EndMap(testdata::TestValue* map_value) {
  for (auto& expr : *map_value->mutable_expr()) {
    if (absl::EndsWith(expr, ", ")) {
      expr.erase(expr.size() - 2);
    }
    absl::StrAppend(&expr, "}");
  }
}

}  // namespace internal

TestValue WithName(TestValue value, absl::string_view name) {
  value.set_name(std::string(name));
  return value;
}

TestValue Merge(const absl::Span<const TestValue>& values,
                absl::string_view name) {
  TestValue result;
  for (const auto& value : values) {
    if (!value.name().empty()) {
      result.set_name(std::string(value.name()));
    }
    result.mutable_v1beta1()->MergeFrom(value.v1beta1());
    result.mutable_expr()->MergeFrom(value.expr());
    result.mutable_proto()->MergeFrom(value.proto());
  }
  if (!name.empty()) {
    result.set_name(std::string(name));
  }
  return result;
}

}  // namespace testutil
}  // namespace expr
}  // namespace api
}  // namespace google
