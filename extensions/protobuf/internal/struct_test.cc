// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "extensions/protobuf/internal/struct.h"

#include <memory>

#include "google/protobuf/struct.pb.h"
#include "google/protobuf/descriptor.pb.h"
#include "absl/log/absl_check.h"
#include "absl/memory/memory.h"
#include "common/json.h"
#include "internal/testing.h"
#include "testutil/util.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/text_format.h"

namespace cel::extensions::protobuf_internal {
namespace {

using ::google::api::expr::testutil::EqualsProto;
using testing::IsEmpty;
using testing::VariantWith;
using cel::internal::IsOkAndHolds;

template <typename T>
T ParseTextOrDie(absl::string_view text) {
  T proto;
  ABSL_CHECK(google::protobuf::TextFormat::ParseFromString(text, &proto));
  return proto;
}

std::unique_ptr<google::protobuf::Message> ParseTextOrDie(
    absl::string_view text, const google::protobuf::Message& prototype) {
  auto message = absl::WrapUnique(prototype.New());
  ABSL_CHECK(google::protobuf::TextFormat::ParseFromString(text, message.get()));
  return message;
}

TEST(Value, Generated) {
  google::protobuf::Value proto;
  EXPECT_THAT(GeneratedValueProtoToJson(proto),
              IsOkAndHolds(VariantWith<JsonNull>(kJsonNull)));
  proto.set_null_value(google::protobuf::NULL_VALUE);
  EXPECT_THAT(GeneratedValueProtoToJson(proto),
              IsOkAndHolds(VariantWith<JsonNull>(kJsonNull)));
  proto.Clear();
  EXPECT_OK(GeneratedValueProtoFromJson(Json(), proto));
  EXPECT_THAT(proto, EqualsProto(ParseTextOrDie<google::protobuf::Value>(
                         R"pb(null_value: 0)pb")));

  proto.set_bool_value(true);
  EXPECT_THAT(GeneratedValueProtoToJson(proto),
              IsOkAndHolds(VariantWith<JsonBool>(true)));
  proto.Clear();
  EXPECT_OK(GeneratedValueProtoFromJson(Json(true), proto));
  EXPECT_THAT(proto, EqualsProto(ParseTextOrDie<google::protobuf::Value>(
                         R"pb(bool_value: true)pb")));

  proto.set_number_value(1.0);
  EXPECT_THAT(GeneratedValueProtoToJson(proto),
              IsOkAndHolds(VariantWith<JsonNumber>(1.0)));
  proto.Clear();
  EXPECT_OK(GeneratedValueProtoFromJson(Json(1.0), proto));
  EXPECT_THAT(proto, EqualsProto(ParseTextOrDie<google::protobuf::Value>(
                         R"pb(number_value: 1.0)pb")));

  proto.set_string_value("foo");
  EXPECT_THAT(GeneratedValueProtoToJson(proto),
              IsOkAndHolds(VariantWith<JsonString>(JsonString("foo"))));
  proto.Clear();
  EXPECT_OK(GeneratedValueProtoFromJson(Json(JsonString("foo")), proto));
  EXPECT_THAT(proto, EqualsProto(ParseTextOrDie<google::protobuf::Value>(
                         R"pb(string_value: "foo")pb")));

  proto.mutable_list_value();
  EXPECT_THAT(GeneratedValueProtoToJson(proto),
              IsOkAndHolds(VariantWith<JsonArray>(IsEmpty())));
  proto.Clear();
  EXPECT_OK(GeneratedValueProtoFromJson(Json(JsonArray()), proto));
  EXPECT_THAT(proto, EqualsProto(ParseTextOrDie<google::protobuf::Value>(
                         R"pb(list_value: {})pb")));

  proto.mutable_struct_value();
  EXPECT_THAT(GeneratedValueProtoToJson(proto),
              IsOkAndHolds(VariantWith<JsonObject>(IsEmpty())));
  proto.Clear();
  EXPECT_OK(GeneratedValueProtoFromJson(Json(JsonObject()), proto));
  EXPECT_THAT(proto, EqualsProto(ParseTextOrDie<google::protobuf::Value>(
                         R"pb(struct_value: {})pb")));
}

TEST(Value, Dynamic) {
  google::protobuf::SimpleDescriptorDatabase database;
  {
    google::protobuf::FileDescriptorProto fd;
    google::protobuf::Value::descriptor()->file()->CopyTo(&fd);
    ASSERT_TRUE(database.Add(fd));
  }
  google::protobuf::DescriptorPool pool(&database);
  pool.AllowUnknownDependencies();
  google::protobuf::DynamicMessageFactory factory(&pool);
  factory.SetDelegateToGeneratedFactory(false);
  std::unique_ptr<google::protobuf::Message> proto = absl::WrapUnique(
      factory.GetPrototype(pool.FindMessageTypeByName("google.protobuf.Value"))
          ->New());
  const auto* reflection = proto->GetReflection();
  const auto* descriptor = proto->GetDescriptor();

  EXPECT_THAT(DynamicValueProtoToJson(*proto),
              IsOkAndHolds(VariantWith<JsonNull>(kJsonNull)));
  reflection->SetEnumValue(proto.get(),
                           descriptor->FindFieldByName("null_value"), 0);
  EXPECT_THAT(DynamicValueProtoToJson(*proto),
              IsOkAndHolds(VariantWith<JsonNull>(kJsonNull)));
  proto->Clear();
  EXPECT_OK(DynamicValueProtoFromJson(Json(), *proto));
  EXPECT_THAT(*proto,
              EqualsProto(*ParseTextOrDie(R"pb(null_value: 0)pb", *proto)));

  reflection->SetBool(proto.get(), descriptor->FindFieldByName("bool_value"),
                      true);
  EXPECT_THAT(DynamicValueProtoToJson(*proto),
              IsOkAndHolds(VariantWith<JsonBool>(true)));
  proto->Clear();
  EXPECT_OK(DynamicValueProtoFromJson(Json(true), *proto));
  EXPECT_THAT(*proto,
              EqualsProto(*ParseTextOrDie(R"pb(bool_value: true)pb", *proto)));

  reflection->SetDouble(proto.get(),
                        descriptor->FindFieldByName("number_value"), 1.0);
  EXPECT_THAT(DynamicValueProtoToJson(*proto),
              IsOkAndHolds(VariantWith<JsonNumber>(1.0)));
  proto->Clear();
  EXPECT_OK(DynamicValueProtoFromJson(Json(1.0), *proto));
  EXPECT_THAT(*proto,
              EqualsProto(*ParseTextOrDie(R"pb(number_value: 1.0)pb", *proto)));

  reflection->SetString(proto.get(),
                        descriptor->FindFieldByName("string_value"), "foo");
  EXPECT_THAT(DynamicValueProtoToJson(*proto),
              IsOkAndHolds(VariantWith<JsonString>(JsonString("foo"))));
  proto->Clear();
  EXPECT_OK(DynamicValueProtoFromJson(Json(JsonString("foo")), *proto));
  EXPECT_THAT(*proto, EqualsProto(*ParseTextOrDie(R"pb(string_value: "foo")pb",
                                                  *proto)));

  reflection->MutableMessage(
      proto.get(), descriptor->FindFieldByName("list_value"), &factory);
  EXPECT_THAT(DynamicValueProtoToJson(*proto),
              IsOkAndHolds(VariantWith<JsonArray>(IsEmpty())));
  proto->Clear();
  EXPECT_OK(DynamicValueProtoFromJson(Json(JsonArray()), *proto));
  EXPECT_THAT(*proto,
              EqualsProto(*ParseTextOrDie(R"pb(list_value: {})pb", *proto)));

  reflection->MutableMessage(
      proto.get(), descriptor->FindFieldByName("struct_value"), &factory);
  EXPECT_THAT(DynamicValueProtoToJson(*proto),
              IsOkAndHolds(VariantWith<JsonObject>(IsEmpty())));
  EXPECT_OK(DynamicValueProtoFromJson(Json(JsonObject()), *proto));
  EXPECT_THAT(*proto,
              EqualsProto(*ParseTextOrDie(R"pb(struct_value: {})pb", *proto)));
}

}  // namespace
}  // namespace cel::extensions::protobuf_internal
