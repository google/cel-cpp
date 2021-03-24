#include "google/protobuf/empty.pb.h"
#include "google/rpc/code.pb.h"
#include "google/type/money.pb.h"
#include "absl/flags/parse.h"
#include "absl/strings/match.h"
#include "common/type.h"
#include "internal/proto_util.h"
#include "testutil/test_data_io.h"
#include "testutil/test_data_util.h"
#include "testdata/test_data.pb.h"

namespace google {
namespace api {
namespace expr {
namespace testutil {

using testdata::TestData;

constexpr int64_t kDoubleIntegerMax = 1L << 53;

/**
 * Constructs test data that enumerate values that are non-equivalent (but may
 * be interpreted as equal)
 */
TestData UniqueValues() {
  TestData values;
  values.set_description("Set of unique (not equivalent) test values.");
  auto add_val = ::google::protobuf::RepeatedFieldBackInserter(
      values.mutable_test_values()->mutable_values());

  // Null.
  add_val = NewValue(nullptr);

  // Bool.
  add_val = NewValue(true);
  add_val = NewValue(false);

  // Int64
  add_val = NewValue(std::numeric_limits<int64_t>::min(), "min");
  add_val = NewValue(-kDoubleIntegerMax - 1, "max_double_int-1");
  add_val = NewValue(-kDoubleIntegerMax, "max_double_int");
  add_val = NewValue(-1);
  add_val = NewValue(0);
  add_val = NewValue(1);
  add_val = NewValue(kDoubleIntegerMax, "max_double_int");
  add_val = NewValue(kDoubleIntegerMax + 1, "max_double_int+1");
  add_val = NewValue(std::numeric_limits<int64_t>::max(), "max");

  // Uint64
  add_val = NewValue(0u);
  add_val = NewValue(1u);
  add_val =
      NewValue(static_cast<uint64_t>(kDoubleIntegerMax), "max_double_int"),
  add_val = NewValue(static_cast<uint64_t>(kDoubleIntegerMax) + 1,
                     "max_double_int+1");
  add_val = NewValue(std::numeric_limits<uint64_t>::max(), "max");

  // Double
  // NAN is 'equivalent' to itself, but not equal to itself.
  add_val = NewValue(NAN, "NaN");
  add_val = NewValue(-std::numeric_limits<double>::infinity(), "-inf"),
  add_val = NewValue(std::numeric_limits<double>::min(), "min"),
  add_val =
      NewValue(-static_cast<double>(kDoubleIntegerMax) - 2, "max_double_int-2");
  add_val = NewValue(-static_cast<double>(kDoubleIntegerMax), "max_double_int");
  add_val = NewValue(-1.0);
  add_val = NewValue(0.0);
  add_val = NewValue(1.0);
  add_val = NewValue(static_cast<double>(kDoubleIntegerMax), "max_double_int");
  add_val =
      NewValue(static_cast<double>(kDoubleIntegerMax) + 2, "max_double_int+2");
  add_val = NewValue(std::numeric_limits<double>::max(), "max"),
  add_val = NewValue(std::numeric_limits<double>::infinity(), "+inf"),

  // String
      add_val = NewValue("", "empty");
  add_val = NewValue("hi");

  // Bytes
  add_val = NewBytesValue("", "empty");
  add_val = NewBytesValue("hi");

  // Duration
  add_val = NewValue(expr::internal::MakeGoogleApiDurationMin(), "min");
  add_val = NewValue(
      expr::internal::MakeGoogleApiDurationMin() + absl::Nanoseconds(1),
      "min+1");
  add_val = NewValue(absl::Nanoseconds(-1), "-1");
  add_val = NewValue(absl::ZeroDuration(), "0");
  add_val = NewValue(absl::Nanoseconds(1), "1");
  add_val = NewValue(
      expr::internal::MakeGoogleApiDurationMax() - absl::Nanoseconds(1),
      "max-1");
  add_val = NewValue(expr::internal::MakeGoogleApiDurationMax(), "max");

  // Timestmap
  add_val = NewValue(expr::internal::MakeGoogleApiTimeMin(), "min");
  add_val = NewValue(
      expr::internal::MakeGoogleApiTimeMin() + absl::Nanoseconds(1), "min+1");
  add_val = NewValue(absl::FromUnixNanos(-1), "-1");
  add_val = NewValue(absl::FromUnixNanos(0), "0");
  add_val = NewValue(absl::FromUnixNanos(1), "1");
  add_val = NewValue(
      expr::internal::MakeGoogleApiTimeMax() - absl::Nanoseconds(1), "max-1");
  add_val = NewValue(expr::internal::MakeGoogleApiTimeMax(), "max");

  // Message
  add_val = NewValue(google::protobuf::Empty(), "empty");
  google::type::Money money;
  add_val = NewValue(money, "empty");
  money.set_nanos(100);
  add_val = NewValue(money, "100");
  money.set_nanos(5);
  add_val = NewValue(money, "5");

  // List
  add_val = WithName(NewListValue(), "list(empty)");
  add_val = WithName(NewListValue(1), "list<int>");
  add_val = WithName(NewListValue(true, true, false), "list<bool>");
  add_val = WithName(NewListValue(nullptr, nullptr), "list<null>");
  add_val = WithName(NewListValue(absl::Seconds(1), absl::Seconds(2)),
                     "list<duration>");
  add_val = WithName(NewListValue(money, money), "list<money>");
  add_val = WithName(NewListValue(1u), "list<uint>");
  add_val = WithName(NewListValue(1.0), "list<double>");
  add_val = WithName(NewListValue(1.0, 2.0), "list<double>");
  add_val = WithName(NewListValue(2.0, 1.0), "list<double>");
  add_val = WithName(NewListValue("hi", "bye"), "list<string>");
  add_val = WithName(NewListValue(1.0, "hi"), "list<double|string>");
  add_val = WithName(NewListValue("hi", 1.0), "list<string|double>");
  add_val = WithName(NewListValue(1, "hi"), "list<int|string>");
  add_val = WithName(NewListValue("hi", 1), "list<string|int>");
  add_val = WithName(NewListValue(1u, "hi"), "list<uint|string>");
  add_val = WithName(NewListValue(true, "hi"), "list<bool|string>");

  // Map
  add_val = WithName(NewMapValue(), "map(empty)");
  add_val = WithName(NewMapValue(1, "hi"), "map<int, string>");
  add_val = WithName(NewMapValue(1u, "hi"), "map<uint, string>");
  add_val = WithName(NewMapValue(1.0, "hi"), "map<double, string>");
  add_val = WithName(NewMapValue(true, "hi"), "map<bool, string>");
  add_val = WithName(NewMapValue("hi", 1), "map<string, int>");
  add_val = WithName(NewMapValue("hi", 1u), "map<string, uint>");
  add_val = WithName(NewMapValue("hi", 1.0), "map<string, double>");
  add_val = WithName(NewMapValue("hi", true), "map<string, bool>");
  add_val = WithName(NewMapValue("hi", "bye"), "map<string, string>");
  add_val = Merge({NewMapValue("hi", "bye", "foo", "bar"),
                   NewMapValue("foo", "bar", "hi", "bye")},
                  "multiple_orderings");

  add_val =
      NewValue(expr::common::BasicType(expr::common::BasicTypeValue::kInt));
  add_val = NewValue(expr::common::Type("google.protobuf.Duration"));
  add_val = NewValue(expr::common::Type("unknown.type"));
  return values;
}

void WriteData() {
  auto status = WriteTestData("unique_values", UniqueValues());
  GOOGLE_CHECK(status.code() == google::rpc::Code::OK) << status.ShortDebugString();
}

}  // namespace testutil
}  // namespace expr
}  // namespace api
}  // namespace google

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  google::api::expr::testutil::WriteData();
}
