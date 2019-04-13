#include "eval/public/activation.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

absl::optional<CelValue> Activation::FindValue(absl::string_view name,
                                               google::protobuf::Arena* arena) const {
  auto entry = value_map_.find(std::string(name));

  // No entry found.
  if (entry == value_map_.end()) {
    return {};
  }

  return entry->second.RetrieveValue(arena);
}

void Activation::InsertValue(absl::string_view name, const CelValue& value) {
  value_map_.emplace(std::string(name), ValueEntry(value));
}

void Activation::InsertValueProducer(
    absl::string_view name, std::unique_ptr<CelValueProducer> value_producer) {
  value_map_.emplace(std::string(name), ValueEntry(std::move(value_producer)));
}

bool Activation::RemoveValueEntry(absl::string_view name) {
  return value_map_.erase(std::string(name));
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
