#include "eval/eval/field_backed_map_impl.h"
#include "eval/testutil/test_message.pb.h"
#include "absl/strings/str_cat.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {
namespace {

using testing::Eq;
using testing::UnorderedPointwise;

// Helper method. Creates simple pipeline containing Select step and runs it.
std::unique_ptr<CelMap> CreateMap(const TestMessage* message,
                                  const std::string& field, google::protobuf::Arena* arena) {
  const google::protobuf::FieldDescriptor* field_desc =
      message->GetDescriptor()->FindFieldByName(field);

  return absl::make_unique<FieldBackedMapImpl>(message, field_desc, arena);
}

TEST(FieldBackedMapImplTest, IntKeyTest) {
  TestMessage message;
  auto field_map = message.mutable_int64_int32_map();
  (*field_map)[0] = 1;
  (*field_map)[1] = 2;

  google::protobuf::Arena arena;

  auto cel_map = CreateMap(&message, "int64_int32_map", &arena);

  EXPECT_EQ((*cel_map)[CelValue::CreateInt64(0)]->Int64OrDie(), 1);
  EXPECT_EQ((*cel_map)[CelValue::CreateInt64(1)]->Int64OrDie(), 2);

  // Look up nonexistent key
  EXPECT_EQ((*cel_map)[CelValue::CreateInt64(3)].has_value(), false);
}

TEST(FieldBackedMapImplTest, UintKeyTest) {
  TestMessage message;
  auto field_map = message.mutable_uint64_int32_map();
  (*field_map)[0] = 1;
  (*field_map)[1] = 2;

  google::protobuf::Arena arena;

  auto cel_map = CreateMap(&message, "uint64_int32_map", &arena);

  EXPECT_EQ((*cel_map)[CelValue::CreateUint64(0)]->Int64OrDie(), 1);
  EXPECT_EQ((*cel_map)[CelValue::CreateUint64(1)]->Int64OrDie(), 2);

  // Look up nonexistent key
  EXPECT_EQ((*cel_map)[CelValue::CreateUint64(3)].has_value(), false);
}

TEST(FieldBackedMapImplTest, StringKeyTest) {
  TestMessage message;
  auto field_map = message.mutable_string_int32_map();
  (*field_map)["test0"] = 1;
  (*field_map)["test1"] = 2;

  google::protobuf::Arena arena;

  auto cel_map = CreateMap(&message, "string_int32_map", &arena);

  std::string test0 = "test0";
  std::string test1 = "test1";
  std::string test_notfound = "test_notfound";

  EXPECT_EQ((*cel_map)[CelValue::CreateString(&test0)]->Int64OrDie(), 1);
  EXPECT_EQ((*cel_map)[CelValue::CreateString(&test1)]->Int64OrDie(), 2);

  // Look up nonexistent key
  EXPECT_EQ((*cel_map)[CelValue::CreateString(&test_notfound)].has_value(),
            false);
}

TEST(FieldBackedMapImplTest, EmptySizeTest) {
  TestMessage message;

  google::protobuf::Arena arena;

  auto cel_map = CreateMap(&message, "string_int32_map", &arena);

  std::string test0 = "test0";
  std::string test1 = "test1";

  EXPECT_EQ(cel_map->size(), 0);
}

TEST(FieldBackedMapImplTest, RepeatedAddTest) {
  TestMessage message;
  auto field_map = message.mutable_string_int32_map();
  (*field_map)["test0"] = 1;
  (*field_map)["test1"] = 2;
  (*field_map)["test0"] = 3;

  google::protobuf::Arena arena;

  auto cel_map = CreateMap(&message, "string_int32_map", &arena);

  EXPECT_EQ(cel_map->size(), 2);
}

TEST(FieldBackedMapImplTest, KeyListTest) {
  TestMessage message;
  auto field_map = message.mutable_string_int32_map();
  std::vector<std::string> keys;
  std::vector<std::string> keys1;
  for (int i = 0; i < 100; i++) {
    keys.push_back(absl::StrCat("test", i));
    (*field_map)[keys.back()] = i;
  }

  google::protobuf::Arena arena;

  auto cel_map = CreateMap(&message, "string_int32_map", &arena);

  const CelList* key_list = cel_map->ListKeys();

  EXPECT_EQ(key_list->size(), 100);
  for (int i = 0; i < key_list->size(); i++) {
    keys1.push_back(std::string((*key_list)[i].StringOrDie().value()));
  }

  EXPECT_THAT(keys, UnorderedPointwise(Eq(), keys1));
}

}  // namespace
}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
