#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_EXPRESSION_BUILD_WARNING_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_EXPRESSION_BUILD_WARNING_H_

#include <utility>
#include <vector>

#include "absl/status/status.h"

namespace google::api::expr::runtime {

// Container for recording warnings.
class BuilderWarnings {
 public:
  explicit BuilderWarnings(bool fail_immediately = false)
      : fail_immediately_(fail_immediately) {}

  // Add a warning. Returns the util:Status immediately if fail on warning is
  // set.
  absl::Status AddWarning(const absl::Status& warning);

  bool fail_immediately() const { return fail_immediately_; }

  // Return the list of recorded warnings.
  const std::vector<absl::Status>& warnings() const& { return warnings_; }

  std::vector<absl::Status>&& warnings() && { return std::move(warnings_); }

 private:
  std::vector<absl::Status> warnings_;
  bool fail_immediately_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_EXPRESSION_BUILD_WARNING_H_
