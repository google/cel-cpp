#include "internal/testing.h"

namespace cel::internal {

void StatusIsMatcherCommonImpl::DescribeTo(std::ostream* os) const {
  *os << ", has a status code that ";
  code_matcher_.DescribeTo(os);
  *os << ", and has an error message that ";
  message_matcher_.DescribeTo(os);
}

void StatusIsMatcherCommonImpl::DescribeNegationTo(std::ostream* os) const {
  *os << ", or has a status code that ";
  code_matcher_.DescribeNegationTo(os);
  *os << ", or has an error message that ";
  message_matcher_.DescribeNegationTo(os);
}

bool StatusIsMatcherCommonImpl::MatchAndExplain(
    const absl::Status& status,
    ::testing::MatchResultListener* result_listener) const {
  ::testing::StringMatchResultListener inner_listener;

  inner_listener.Clear();
  if (!code_matcher_.MatchAndExplain(status.code(), &inner_listener)) {
    *result_listener << (inner_listener.str().empty()
                             ? "whose status code is wrong"
                             : "which has a status code " +
                                   inner_listener.str());
    return false;
  }

  if (!message_matcher_.Matches(std::string(status.message()))) {
    *result_listener << "whose error message is wrong";
    return false;
  }

  return true;
}

void AddFatalFailure(const char* file, int line, absl::string_view expression,
                     const StatusBuilder& builder) {
  GTEST_MESSAGE_AT_(file, line,
                    absl::StrCat(expression, " returned error: ",
                                 absl::Status(builder).ToString(
                                     absl::StatusToStringMode::kWithEverything))
                        .c_str(),
                    ::testing::TestPartResult::kFatalFailure);
}

}  // namespace cel::internal
