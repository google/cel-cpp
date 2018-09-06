#include "internal/cel_printer.h"

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/strings/str_cat.h"

namespace google {
namespace api {
namespace expr {
namespace internal {

constexpr const absl::string_view BaseJoinPolicy::kValueDelim;

constexpr const absl::string_view ListJoinPolicy::kStart;
constexpr const absl::string_view ListJoinPolicy::kEnd;
constexpr const absl::string_view SetJoinPolicy::kStart;
constexpr const absl::string_view SetJoinPolicy::kEnd;
constexpr const absl::string_view CallJoinPolicy::kStart;
constexpr const absl::string_view CallJoinPolicy::kEnd;

constexpr const absl::string_view MapJoinPolicy::kKeyDelim;
constexpr const absl::string_view ObjectJoinPolicy::kKeyDelim;

std::string ScalarPrinter::operator()(absl::Time value) {
  return ToCallString(
      google::protobuf::Timestamp::descriptor()->full_name(),
      absl::FormatTime("%Y-%m-%dT%H:%M:%E*SZ", value, absl::UTCTimeZone()));
}

std::string ScalarPrinter::operator()(absl::Duration value) {
  return ToCallString(google::protobuf::Duration::descriptor()->full_name(),
                      absl::FormatDuration(value));
}

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google
