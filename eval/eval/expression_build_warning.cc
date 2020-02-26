#include "eval/eval/expression_build_warning.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

absl::Status BuilderWarnings::AddWarning(const absl::Status& warning) {
  if (fail_immediately_) {
    return warning;
  }
  warnings_.push_back(warning);
  return absl::OkStatus();
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
