#include "eval/eval/expression_build_warning.h"

namespace google::api::expr::runtime {

absl::Status BuilderWarnings::AddWarning(const absl::Status& warning) {
  if (fail_immediately_) {
    return warning;
  }
  warnings_.push_back(warning);
  return absl::OkStatus();
}

}  // namespace google::api::expr::runtime
