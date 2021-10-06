#include "eval/eval/expression_build_warning.h"

#include "absl/status/status.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {
namespace {

using cel::internal::IsOk;

TEST(BuilderWarnings, NoFailCollects) {
  BuilderWarnings warnings(false);

  auto status = warnings.AddWarning(absl::InternalError("internal"));
  EXPECT_THAT(status, IsOk());
  auto status2 = warnings.AddWarning(absl::InternalError("internal error 2"));
  EXPECT_THAT(status2, IsOk());

  EXPECT_THAT(warnings.warnings(), testing::SizeIs(2));
}

TEST(BuilderWarnings, FailReturnsStatus) {
  BuilderWarnings warnings(true);

  EXPECT_EQ(warnings.AddWarning(absl::InternalError("internal")).code(),
            absl::StatusCode::kInternal);
}

}  // namespace
}  // namespace google::api::expr::runtime
