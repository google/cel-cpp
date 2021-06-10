#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_CONTAINER_BACKED_MAP_IMPL_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_CONTAINER_BACKED_MAP_IMPL_H_

#include <memory>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Template factory method creating container-backed CelMap.
absl::StatusOr<std::unique_ptr<CelMap>> CreateContainerBackedMap(
    absl::Span<std::pair<CelValue, CelValue>> key_values);

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_CONTAINER_BACKED_MAP_IMPL_H_
