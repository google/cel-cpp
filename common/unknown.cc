#include "common/unknown.h"
#include "common/macros.h"

namespace google {
namespace api {
namespace expr {
namespace common {

Unknown::Unknown(Id id) { ids_.insert(id); }

Unknown::Unknown(absl::Span<const Id> ids) {
  assert(ids.begin() != ids.end());
  ids_.insert(ids.begin(), ids.end());
}

std::size_t Unknown::hash_code() const {
  std::size_t code = internal::kIntegralTypeOffset;
  for (const auto& id : ids_) {
    internal::AccumulateHashNoOrder(id, &code);
  }
  return code;
}

std::string Unknown::ToString() const {
  internal::SequencePrinter<internal::SetJoinPolicy> printer;
  return printer("Unknown", ids_);
}

}  // namespace common
}  // namespace expr
}  // namespace api
}  // namespace google
