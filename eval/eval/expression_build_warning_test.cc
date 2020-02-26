#include "eval/eval/expression_build_warning.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {
namespace {


TEST(BuilderWarnings, NoFailCollects) {
  BuilderWarnings warnings(false);

  auto status = warnings.AddWarning(absl::InternalError("internal"));
  EXPECT_TRUE(status.ok());
  auto status2 = warnings.AddWarning(absl::InternalError("internal error 2"));
  EXPECT_TRUE(status2.ok());

  EXPECT_THAT(warnings.warnings(), testing::SizeIs(2));
}

TEST(BuilderWarnings, FailReturnsStatus) {
  BuilderWarnings warnings(true);

  EXPECT_EQ(warnings.AddWarning(absl::InternalError("internal")).code(),
            absl::StatusCode::kInternal);
}

}  // namespace
}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
