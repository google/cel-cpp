#include "eval/public/cel_function.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/function_interface.h"
#include "eval/internal/interop.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {

using ::cel::FunctionEvaluationContext;
using ::cel::Handle;
using ::cel::Value;
using ::cel::extensions::ProtoMemoryManager;
using ::cel::interop_internal::ModernValueToLegacyValueOrDie;

bool CelFunction::MatchArguments(absl::Span<const CelValue> arguments) const {
  auto types_size = descriptor().types().size();

  if (types_size != arguments.size()) {
    return false;
  }
  for (size_t i = 0; i < types_size; i++) {
    const auto& value = arguments[i];
    CelValue::Type arg_type = descriptor().types()[i];
    if (value.type() != arg_type && arg_type != CelValue::Type::kAny) {
      return false;
    }
  }

  return true;
}

bool CelFunction::MatchArguments(
    absl::Span<const cel::Handle<cel::Value>> arguments) const {
  auto types_size = descriptor().types().size();

  if (types_size != arguments.size()) {
    return false;
  }
  for (size_t i = 0; i < types_size; i++) {
    const auto& value = arguments[i];
    CelValue::Type arg_type = descriptor().types()[i];
    if (value->kind() != arg_type && arg_type != CelValue::Type::kAny) {
      return false;
    }
  }

  return true;
}

absl::StatusOr<Handle<Value>> CelFunction::Invoke(
    const FunctionEvaluationContext& context,
    absl::Span<const Handle<Value>> arguments) const {
  google::protobuf::Arena* arena = ProtoMemoryManager::CastToProtoArena(
      context.value_factory().memory_manager());
  std::vector<CelValue> legacy_args = ModernValueToLegacyValueOrDie(
      context.value_factory().memory_manager(), arguments);
  CelValue legacy_result;

  CEL_RETURN_IF_ERROR(Evaluate(legacy_args, &legacy_result, arena));

  return cel::interop_internal::LegacyValueToModernValueOrDie(arena,
                                                              legacy_result);
}

}  // namespace google::api::expr::runtime
