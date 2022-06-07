#include "eval/eval/expression_build_warning.h"

namespace google::api::expr::runtime {

absl::Status BuilderWarnings::AddWarning(const absl::Status& warning) {
  // Track errors
  warnings_.push_back(warning);

  if (fail_immediately_) {
    return warning;
  }

  return absl::OkStatus();
}

}  // namespace google::api::expr::runtime
