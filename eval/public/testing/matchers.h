#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_TESTING_MATCHERS_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_TESTING_MATCHERS_H_

#include <ostream>

#include "google/protobuf/message.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "eval/public/cel_value.h"
#include "eval/public/set_util.h"
#include "eval/public/testing/debug_string.h"
#include "eval/public/unknown_set.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// GTest Printer
void PrintTo(const CelValue& value, std::ostream* os);

namespace test {

// readability alias
using CelValueMatcher = testing::Matcher<CelValue>;

// Tests equality to CelValue v using the set_util implementation.
CelValueMatcher EqualsCelValue(const CelValue& v);

// Matches CelValues of type bool whose held value matches |m|.
CelValueMatcher IsCelBool(testing::Matcher<bool> m);

// Matches CelValues of type int64_t whose held value matches |m|.
CelValueMatcher IsCelInt64(testing::Matcher<int64_t> m);

// Matches CelValues of type uint64_t whose held value matches |m|.
CelValueMatcher IsCelUint64(testing::Matcher<uint64_t> m);

// Matches CelValues of type double whose held value matches |m|.
CelValueMatcher IsCelDouble(testing::Matcher<double> m);

// Matches CelValues of type string whose held value matches |m|.
CelValueMatcher IsCelString(testing::Matcher<absl::string_view> m);

// Matches CelValues of type bytes whose held value matches |m|.
CelValueMatcher IsCelBytes(testing::Matcher<absl::string_view> m);

// Matches CelValues of type message whose held value matches |m|.
CelValueMatcher IsCelMessage(testing::Matcher<const google::protobuf::Message*> m);

// Matches CelValues of type duration whose held value matches |m|.
CelValueMatcher IsCelDuration(testing::Matcher<absl::Duration> m);

// Matches CelValues of type timestamp whose held value matches |m|.
CelValueMatcher IsCelTimestamp(testing::Matcher<absl::Time> m);

// Matches CelValues of type error whose held value matches |m|.
// The matcher |m| is wrapped to allow using the testing::status::... matchers.
CelValueMatcher IsCelError(testing::Matcher<absl::Status> m);

// TODO(issues/73): add helpers for working with maps, unknown sets, and
// lists.

}  // namespace test
}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_TESTING_MATCHERS_H_
