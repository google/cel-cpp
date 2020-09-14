#include "eval/public/unknown_function_result_set.h"

#include <sys/ucontext.h>

#include <memory>

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/empty.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/arena.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/structs/cel_proto_wrapper.h"
namespace google {
namespace api {
namespace expr {
namespace runtime {
namespace {

using ::google::protobuf::ListValue;
using ::google::protobuf::Struct;
using ::google::protobuf::Arena;
using testing::Eq;
using testing::SizeIs;

CelFunctionDescriptor kTwoInt("TwoInt", false,
                              {CelValue::Type::kInt64, CelValue::Type::kInt64});

CelFunctionDescriptor kOneInt("OneInt", false, {CelValue::Type::kInt64});

// Helper to confirm the set comparator works.
bool IsLessThan(const UnknownFunctionResult& lhs,
                const UnknownFunctionResult& rhs) {
  return UnknownFunctionComparator()(&lhs, &rhs);
}

TEST(UnknownFunctionResult, ArgumentCapture) {
  UnknownFunctionResult call1(
      kTwoInt, /*expr_id=*/0,
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2)});

  EXPECT_THAT(call1.arguments(), SizeIs(2));
  EXPECT_THAT(call1.arguments().at(0).Int64OrDie(), Eq(1));
}

TEST(UnknownFunctionResult, Equals) {
  UnknownFunctionResult call1(
      kTwoInt, /*expr_id=*/0,
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2)});

  UnknownFunctionResult call2(
      kTwoInt, /*expr_id=*/0,
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2)});

  EXPECT_TRUE(call1.IsEqualTo(call2));
  EXPECT_FALSE(IsLessThan(call1, call2));
  EXPECT_FALSE(IsLessThan(call2, call1));

  UnknownFunctionResult call3(kOneInt, /*expr_id=*/0,
                              {CelValue::CreateInt64(1)});

  UnknownFunctionResult call4(kOneInt, /*expr_id=*/0,
                              {CelValue::CreateInt64(1)});

  EXPECT_TRUE(call3.IsEqualTo(call4));
}

TEST(UnknownFunctionResult, InequalDescriptor) {
  UnknownFunctionResult call1(
      kTwoInt, /*expr_id=*/0,
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2)});

  UnknownFunctionResult call2(kOneInt, /*expr_id=*/0,
                              {CelValue::CreateInt64(1)});

  EXPECT_FALSE(call1.IsEqualTo(call2));
  EXPECT_TRUE(IsLessThan(call2, call1));

  CelFunctionDescriptor one_uint("OneInt", false, {CelValue::Type::kUint64});

  UnknownFunctionResult call3(kOneInt, /*expr_id=*/0,
                              {CelValue::CreateInt64(1)});

  UnknownFunctionResult call4(one_uint, /*expr_id=*/0,
                              {CelValue::CreateUint64(1)});

  EXPECT_FALSE(call3.IsEqualTo(call4));
  EXPECT_TRUE(IsLessThan(call3, call4));
}

TEST(UnknownFunctionResult, InequalArgs) {
  UnknownFunctionResult call1(
      kTwoInt, /*expr_id=*/0,
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2)});

  UnknownFunctionResult call2(
      kTwoInt, /*expr_id=*/0,
      {CelValue::CreateInt64(1), CelValue::CreateInt64(3)});

  EXPECT_FALSE(call1.IsEqualTo(call2));
  EXPECT_TRUE(IsLessThan(call1, call2));

  UnknownFunctionResult call3(
      kTwoInt, /*expr_id=*/0,
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2)});

  UnknownFunctionResult call4(kTwoInt, /*expr_id=*/0,
                              {CelValue::CreateInt64(1)});

  EXPECT_FALSE(call3.IsEqualTo(call4));
  EXPECT_TRUE(IsLessThan(call4, call3));
}

TEST(UnknownFunctionResult, ListsEqual) {
  ContainerBackedListImpl cel_list_1(std::vector<CelValue>{
      CelValue::CreateInt64(1), CelValue::CreateInt64(2)});

  ContainerBackedListImpl cel_list_2(std::vector<CelValue>{
      CelValue::CreateInt64(1), CelValue::CreateInt64(2)});

  CelFunctionDescriptor desc("OneList", false, {CelValue::Type::kList});

  UnknownFunctionResult call1(desc, /*expr_id=*/0,
                              {CelValue::CreateList(&cel_list_1)});
  UnknownFunctionResult call2(desc, /*expr_id=*/0,
                              {CelValue::CreateList(&cel_list_2)});

  // [1, 2] == [1, 2]
  EXPECT_TRUE(call1.IsEqualTo(call2));
}

TEST(UnknownFunctionResult, ListsDifferentSizes) {
  ContainerBackedListImpl cel_list_1(std::vector<CelValue>{
      CelValue::CreateInt64(1), CelValue::CreateInt64(2)});

  ContainerBackedListImpl cel_list_2(std::vector<CelValue>{
      CelValue::CreateInt64(1),
      CelValue::CreateInt64(2),
      CelValue::CreateInt64(3),
  });

  CelFunctionDescriptor desc("OneList", false, {CelValue::Type::kList});

  UnknownFunctionResult call1(desc, /*expr_id=*/0,
                              {CelValue::CreateList(&cel_list_1)});
  UnknownFunctionResult call2(desc, /*expr_id=*/0,
                              {CelValue::CreateList(&cel_list_2)});

  // [1, 2] == [1, 2, 3]
  EXPECT_FALSE(call1.IsEqualTo(call2));
  EXPECT_TRUE(IsLessThan(call1, call2));
}

TEST(UnknownFunctionResult, ListsDifferentMembers) {
  ContainerBackedListImpl cel_list_1(std::vector<CelValue>{
      CelValue::CreateInt64(1), CelValue::CreateInt64(2)});

  ContainerBackedListImpl cel_list_2(std::vector<CelValue>{
      CelValue::CreateInt64(2), CelValue::CreateInt64(2)});

  CelFunctionDescriptor desc("OneList", false, {CelValue::Type::kList});

  UnknownFunctionResult call1(desc, /*expr_id=*/0,
                              {CelValue::CreateList(&cel_list_1)});
  UnknownFunctionResult call2(desc, /*expr_id=*/0,
                              {CelValue::CreateList(&cel_list_2)});

  // [1, 2] == [2, 2]
  EXPECT_FALSE(call1.IsEqualTo(call2));
  EXPECT_TRUE(IsLessThan(call1, call2));
}

TEST(UnknownFunctionResult, MapsEqual) {
  std::vector<std::pair<CelValue, CelValue>> values{
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2)},
      {CelValue::CreateInt64(2), CelValue::CreateInt64(4)}};

  auto cel_map_1 = CreateContainerBackedMap(absl::MakeSpan(values));
  auto cel_map_2 = CreateContainerBackedMap(absl::MakeSpan(values));

  CelFunctionDescriptor desc("OneMap", false, {CelValue::Type::kMap});

  UnknownFunctionResult call1(desc, /*expr_id=*/0,
                              {CelValue::CreateMap(cel_map_1.get())});
  UnknownFunctionResult call2(desc, /*expr_id=*/0,
                              {CelValue::CreateMap(cel_map_2.get())});

  // {1: 2, 2: 4} == {1: 2, 2: 4}
  EXPECT_TRUE(call1.IsEqualTo(call2));
}

TEST(UnknownFunctionResult, MapsDifferentSizes) {
  std::vector<std::pair<CelValue, CelValue>> values{
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2)},
      {CelValue::CreateInt64(2), CelValue::CreateInt64(4)}};

  auto cel_map_1 = CreateContainerBackedMap(absl::MakeSpan(values));

  std::vector<std::pair<CelValue, CelValue>> values2{
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2)},
      {CelValue::CreateInt64(2), CelValue::CreateInt64(4)},
      {CelValue::CreateInt64(3), CelValue::CreateInt64(6)}};

  auto cel_map_2 = CreateContainerBackedMap(absl::MakeSpan(values2));

  CelFunctionDescriptor desc("OneMap", false, {CelValue::Type::kMap});

  UnknownFunctionResult call1(desc, /*expr_id=*/0,
                              {CelValue::CreateMap(cel_map_1.get())});
  UnknownFunctionResult call2(desc, /*expr_id=*/0,
                              {CelValue::CreateMap(cel_map_2.get())});

  // {1: 2, 2: 4} == {1: 2, 2: 4, 3: 6}
  EXPECT_FALSE(call1.IsEqualTo(call2));
  EXPECT_TRUE(IsLessThan(call1, call2));
}

TEST(UnknownFunctionResult, MapsDifferentElements) {
  std::vector<std::pair<CelValue, CelValue>> values{
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2)},
      {CelValue::CreateInt64(2), CelValue::CreateInt64(4)},
      {CelValue::CreateInt64(3), CelValue::CreateInt64(6)}};

  auto cel_map_1 = CreateContainerBackedMap(absl::MakeSpan(values));

  std::vector<std::pair<CelValue, CelValue>> values2{
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2)},
      {CelValue::CreateInt64(2), CelValue::CreateInt64(4)},
      {CelValue::CreateInt64(4), CelValue::CreateInt64(8)}};

  auto cel_map_2 = CreateContainerBackedMap(absl::MakeSpan(values2));

  std::vector<std::pair<CelValue, CelValue>> values3{
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2)},
      {CelValue::CreateInt64(2), CelValue::CreateInt64(4)},
      {CelValue::CreateInt64(3), CelValue::CreateInt64(5)}};

  auto cel_map_3 = CreateContainerBackedMap(absl::MakeSpan(values3));

  CelFunctionDescriptor desc("OneMap", false, {CelValue::Type::kMap});

  UnknownFunctionResult call1(desc, /*expr_id=*/0,
                              {CelValue::CreateMap(cel_map_1.get())});
  UnknownFunctionResult call2(desc, /*expr_id=*/0,
                              {CelValue::CreateMap(cel_map_2.get())});
  UnknownFunctionResult call3(desc, /*expr_id=*/0,
                              {CelValue::CreateMap(cel_map_3.get())});

  // {1: 2, 2: 4, 3: 6} == {1: 2, 2: 4, 4: 8}
  EXPECT_FALSE(call1.IsEqualTo(call2));
  EXPECT_TRUE(IsLessThan(call1, call2));
  // {1: 2, 2: 4, 3: 6} == {1: 2, 2: 4, 3: 5}
  EXPECT_FALSE(call1.IsEqualTo(call3));
  EXPECT_TRUE(IsLessThan(call3, call1));
}

TEST(UnknownFunctionResult, Messages) {
  protobuf::Empty message1;
  protobuf::Empty message2;
  google::protobuf::Arena arena;

  CelFunctionDescriptor desc("OneMessage", false, {CelValue::Type::kMessage});

  UnknownFunctionResult call1(
      desc, /*expr_id=*/0, {CelProtoWrapper::CreateMessage(&message1, &arena)});
  UnknownFunctionResult call2(
      desc, /*expr_id=*/0, {CelProtoWrapper::CreateMessage(&message2, &arena)});
  UnknownFunctionResult call3(
      desc, /*expr_id=*/0, {CelProtoWrapper::CreateMessage(&message1, &arena)});

  // &message1 == &message2
  EXPECT_FALSE(call1.IsEqualTo(call2));

  // &message1 == &message1
  EXPECT_TRUE(call1.IsEqualTo(call3));
}

TEST(UnknownFunctionResult, AnyDescriptor) {
  CelFunctionDescriptor anyDesc("OneAny", false, {CelValue::Type::kAny});

  UnknownFunctionResult callAnyInt1(anyDesc, /*expr_id=*/0,
                                    {CelValue::CreateInt64(2)});
  UnknownFunctionResult callInt(kOneInt, /*expr_id=*/0,
                                {CelValue::CreateInt64(2)});

  UnknownFunctionResult callAnyInt2(anyDesc, /*expr_id=*/0,
                                    {CelValue::CreateInt64(2)});
  UnknownFunctionResult callAnyUint(anyDesc, /*expr_id=*/0,
                                    {CelValue::CreateUint64(2)});

  EXPECT_FALSE(callAnyInt1.IsEqualTo(callInt));
  EXPECT_TRUE(IsLessThan(callAnyInt1, callInt));
  EXPECT_FALSE(callAnyInt1.IsEqualTo(callAnyUint));
  EXPECT_TRUE(IsLessThan(callAnyInt1, callAnyUint));
  EXPECT_TRUE(callAnyInt1.IsEqualTo(callAnyInt2));
}

TEST(UnknownFunctionResult, Strings) {
  CelFunctionDescriptor desc("OneString", false, {CelValue::Type::kString});

  UnknownFunctionResult callStringSmile(desc, /*expr_id=*/0,
                                        {CelValue::CreateStringView("😁")});
  UnknownFunctionResult callStringFrown(desc, /*expr_id=*/0,
                                        {CelValue::CreateStringView("🙁")});
  UnknownFunctionResult callStringSmile2(desc, /*expr_id=*/0,
                                         {CelValue::CreateStringView("😁")});

  EXPECT_TRUE(callStringSmile.IsEqualTo(callStringSmile2));
  EXPECT_FALSE(callStringSmile.IsEqualTo(callStringFrown));
}

TEST(UnknownFunctionResult, DurationHandling) {
  google::protobuf::Arena arena;
  absl::Duration duration1 = absl::Seconds(5);
  protobuf::Duration duration2;
  duration2.set_seconds(5);

  CelFunctionDescriptor durationDesc("OneDuration", false,
                                     {CelValue::Type::kDuration});

  UnknownFunctionResult callDuration1(durationDesc, /*expr_id=*/0,
                                      {CelValue::CreateDuration(duration1)});
  UnknownFunctionResult callDuration2(
      durationDesc, /*expr_id=*/0,
      {CelProtoWrapper::CreateMessage(&duration2, &arena)});
  UnknownFunctionResult callDuration3(
      durationDesc, /*expr_id=*/0,
      {CelProtoWrapper::CreateDuration(&duration2)});

  EXPECT_TRUE(callDuration1.IsEqualTo(callDuration2));
  EXPECT_TRUE(callDuration1.IsEqualTo(callDuration3));
}

TEST(UnknownFunctionResult, TimestampHandling) {
  google::protobuf::Arena arena;
  absl::Time ts1 = absl::FromUnixMillis(1000);
  protobuf::Timestamp ts2;
  ts2.set_seconds(1);

  CelFunctionDescriptor timestampDesc("OneTimestamp", false,
                                      {CelValue::Type::kTimestamp});

  UnknownFunctionResult callTimestamp1(timestampDesc, /*expr_id=*/0,
                                       {CelValue::CreateTimestamp(ts1)});
  UnknownFunctionResult callTimestamp2(
      timestampDesc, /*expr_id=*/0,
      {CelProtoWrapper::CreateMessage(&ts2, &arena)});
  UnknownFunctionResult callTimestamp3(
      timestampDesc, /*expr_id=*/0, {CelProtoWrapper::CreateTimestamp(&ts2)});

  EXPECT_TRUE(callTimestamp1.IsEqualTo(callTimestamp2));
  EXPECT_TRUE(callTimestamp1.IsEqualTo(callTimestamp3));
}

// This tests that the conversion and different map backing implementations are
// compatible with the equality tests.
TEST(UnknownFunctionResult, ProtoStructTreatedAsMap) {
  Arena arena;

  const std::vector<std::string> kFields = {"field1", "field2", "field3"};

  Struct value_struct;

  auto& value1 = (*value_struct.mutable_fields())[kFields[0]];
  value1.set_bool_value(true);

  auto& value2 = (*value_struct.mutable_fields())[kFields[1]];
  value2.set_number_value(1.0);

  auto& value3 = (*value_struct.mutable_fields())[kFields[2]];
  value3.set_string_value("test");

  CelValue proto_struct = CelProtoWrapper::CreateMessage(&value_struct, &arena);
  ASSERT_TRUE(proto_struct.IsMap());

  std::vector<std::pair<CelValue, CelValue>> values{
      {CelValue::CreateStringView(kFields[2]),
       CelValue::CreateStringView("test")},
      {CelValue::CreateStringView(kFields[1]), CelValue::CreateDouble(1.0)},
      {CelValue::CreateStringView(kFields[0]), CelValue::CreateBool(true)}};

  auto backing_map = CreateContainerBackedMap(absl::MakeSpan(values));

  CelValue cel_map = CelValue::CreateMap(backing_map.get());

  CelFunctionDescriptor desc("OneMap", false, {CelValue::Type::kMap});

  UnknownFunctionResult proto_struct_result(desc, /*expr_id=*/0,
                                            {proto_struct});
  UnknownFunctionResult cel_map_result(desc, /*expr_id=*/0, {cel_map});

  EXPECT_TRUE(proto_struct_result.IsEqualTo(cel_map_result));
}

// This tests that the conversion and different map backing implementations are
// compatible with the equality tests.
TEST(UnknownFunctionResult, ProtoListTreatedAsList) {
  Arena arena;

  ListValue list_value;

  list_value.add_values()->set_bool_value(true);
  list_value.add_values()->set_number_value(1.0);
  list_value.add_values()->set_string_value("test");

  CelValue proto_list = CelProtoWrapper::CreateMessage(&list_value, &arena);
  ASSERT_TRUE(proto_list.IsList());

  std::vector<CelValue> list_values{CelValue::CreateBool(true),
                                    CelValue::CreateDouble(1.0),
                                    CelValue::CreateStringView("test")};

  ContainerBackedListImpl list_backing(list_values);

  CelValue cel_list = CelValue::CreateList(&list_backing);

  CelFunctionDescriptor desc("OneList", false, {CelValue::Type::kList});

  UnknownFunctionResult proto_list_result(desc, /*expr_id=*/0, {proto_list});
  UnknownFunctionResult cel_list_result(desc, /*expr_id=*/0, {cel_list});

  EXPECT_TRUE(cel_list_result.IsEqualTo(proto_list_result));
}

TEST(UnknownFunctionResult, NestedProtoTypes) {
  Arena arena;

  ListValue list_value;

  list_value.add_values()->set_bool_value(true);
  list_value.add_values()->set_number_value(1.0);
  list_value.add_values()->set_string_value("test");

  std::vector<CelValue> list_values{CelValue::CreateBool(true),
                                    CelValue::CreateDouble(1.0),
                                    CelValue::CreateStringView("test")};

  ContainerBackedListImpl list_backing(list_values);

  CelValue cel_list = CelValue::CreateList(&list_backing);

  Struct value_struct;

  *(value_struct.mutable_fields()->operator[]("field").mutable_list_value()) =
      list_value;

  std::vector<std::pair<CelValue, CelValue>> values{
      {CelValue::CreateStringView("field"), cel_list}};

  auto backing_map = CreateContainerBackedMap(absl::MakeSpan(values));

  CelValue cel_map = CelValue::CreateMap(backing_map.get());
  CelValue proto_map = CelProtoWrapper::CreateMessage(&value_struct, &arena);

  CelFunctionDescriptor desc("OneMap", false, {CelValue::Type::kMap});

  UnknownFunctionResult cel_map_result(desc, /*expr_id=*/0, {cel_map});
  UnknownFunctionResult proto_struct_result(desc, /*expr_id=*/0, {proto_map});

  EXPECT_TRUE(proto_struct_result.IsEqualTo(cel_map_result));
}

UnknownFunctionResult MakeUnknown(int64_t i) {
  return UnknownFunctionResult(kOneInt, /*expr_id=*/0,
                               {CelValue::CreateInt64(i)});
}

testing::Matcher<const UnknownFunctionResult*> UnknownMatches(
    const UnknownFunctionResult& obj) {
  return testing::Truly([&](const UnknownFunctionResult* to_match) {
    return obj.IsEqualTo(*to_match);
  });
}

TEST(UnknownFunctionResultSet, Merge) {
  UnknownFunctionResult a = MakeUnknown(1);
  UnknownFunctionResult b = MakeUnknown(2);
  UnknownFunctionResult c = MakeUnknown(3);
  UnknownFunctionResult d = MakeUnknown(1);

  UnknownFunctionResultSet a1(&a);
  UnknownFunctionResultSet b1(&b);
  UnknownFunctionResultSet c1(&c);
  UnknownFunctionResultSet d1(&d);

  UnknownFunctionResultSet ab(a1, b1);
  UnknownFunctionResultSet cd(c1, d1);

  UnknownFunctionResultSet merged(ab, cd);

  EXPECT_THAT(merged.unknown_function_results(), SizeIs(3));
  EXPECT_THAT(merged.unknown_function_results(),
              testing::UnorderedElementsAre(
                  UnknownMatches(a), UnknownMatches(b), UnknownMatches(c)));
}

}  // namespace
}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
