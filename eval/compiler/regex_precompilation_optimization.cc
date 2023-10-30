// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "eval/compiler/regex_precompilation_optimization.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "base/ast_internal/ast_impl.h"
#include "base/ast_internal/expr.h"
#include "base/builtins.h"
#include "base/values/string_value.h"
#include "common/native_type.h"
#include "eval/compiler/flat_expr_builder_extensions.h"
#include "eval/eval/compiler_constant_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/regex_match_step.h"
#include "internal/casts.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {
namespace {

using cel::NativeTypeId;
using cel::ast_internal::AstImpl;
using cel::ast_internal::Call;
using cel::ast_internal::Expr;
using cel::ast_internal::Reference;
using cel::internal::down_cast;

using ReferenceMap = absl::flat_hash_map<int64_t, Reference>;

bool IsFunctionOverload(const Expr& expr, absl::string_view function,
                        absl::string_view overload, size_t arity,
                        const ReferenceMap& reference_map) {
  if (!expr.has_call_expr()) {
    return false;
  }
  const auto& call_expr = expr.call_expr();
  if (call_expr.function() != function) {
    return false;
  }
  if (call_expr.args().size() + (call_expr.has_target() ? 1 : 0) != arity) {
    return false;
  }

  // If parse-only and opted in to the optimization, assume this is the intended
  // overload. This will still only change the evaluation plan if the second arg
  // is a constant string.
  if (reference_map.empty()) {
    return true;
  }

  auto reference = reference_map.find(expr.id());
  if (reference != reference_map.end() &&
      reference->second.overload_id().size() == 1 &&
      reference->second.overload_id().front() == overload) {
    return true;
  }
  return false;
}

// Abstraction for deduplicating regular expressions over the course of a single
// create expression call. Should not be used during evaluation. Uses
// std::shared_ptr and std::weak_ptr.
class RegexProgramBuilder final {
 public:
  explicit RegexProgramBuilder(int max_program_size)
      : max_program_size_(max_program_size) {}

  absl::StatusOr<std::shared_ptr<const RE2>> BuildRegexProgram(
      std::string pattern) {
    auto existing = programs_.find(pattern);
    if (existing != programs_.end()) {
      if (auto program = existing->second.lock(); program) {
        return program;
      }
      programs_.erase(existing);
    }
    auto program = std::make_shared<RE2>(pattern);
    if (max_program_size_ > 0 && program->ProgramSize() > max_program_size_) {
      return absl::InvalidArgumentError("exceeded RE2 max program size");
    }
    if (!program->ok()) {
      return absl::InvalidArgumentError(
          "invalid_argument unsupported RE2 pattern for matches");
    }
    programs_.insert({std::move(pattern), program});
    return program;
  }

 private:
  const int max_program_size_;
  absl::flat_hash_map<std::string, std::weak_ptr<const RE2>> programs_;
};

class RegexPrecompilationOptimization : public ProgramOptimizer {
 public:
  explicit RegexPrecompilationOptimization(const ReferenceMap& reference_map,
                                           int regex_max_program_size)
      : reference_map_(reference_map),
        regex_program_builder_(regex_max_program_size) {}

  absl::Status OnPreVisit(PlannerContext& context, const Expr& node) override {
    return absl::OkStatus();
  }

  absl::Status OnPostVisit(PlannerContext& context, const Expr& node) override {
    // Check that this is the correct matches overload instead of a user defined
    // overload.
    if (!IsFunctionOverload(node, cel::builtin::kRegexMatch, "matches_string",
                            2, reference_map_)) {
      return absl::OkStatus();
    }

    const Call& call_expr = node.call_expr();
    const Expr& pattern_expr = call_expr.args().back();

    absl::optional<std::string> pattern =
        GetConstantString(context, pattern_expr);
    if (!pattern.has_value()) {
      return absl::OkStatus();
    }

    CEL_ASSIGN_OR_RETURN(auto program, regex_program_builder_.BuildRegexProgram(
                                           std::move(pattern).value()));

    const Expr& subject_expr =
        call_expr.has_target() ? call_expr.target() : call_expr.args().front();

    if (context.GetSubplan(subject_expr).empty()) {
      // This subexpression was already optimized, nothing to do.
      return absl::OkStatus();
    }

    CEL_ASSIGN_OR_RETURN(ExecutionPath new_plan,
                         context.ExtractSubplan(subject_expr));
    CEL_ASSIGN_OR_RETURN(new_plan.emplace_back(),
                         CreateRegexMatchStep(std::move(program), node.id()));

    return context.ReplaceSubplan(node, std::move(new_plan));
  }

 private:
  absl::optional<std::string> GetConstantString(
      PlannerContext& context, const cel::ast_internal::Expr& expr) const {
    if (expr.has_const_expr() && expr.const_expr().has_string_value()) {
      return expr.const_expr().string_value();
    }

    ExecutionPathView re_plan = context.GetSubplan(expr);
    if (re_plan.size() == 1 && re_plan[0]->GetNativeTypeId() ==
                                   NativeTypeId::For<CompilerConstantStep>()) {
      const auto& constant =
          down_cast<const CompilerConstantStep&>(*re_plan[0]);
      if (constant.value()->Is<cel::StringValue>()) {
        return constant.value()->As<cel::StringValue>().ToString();
      }
    }

    return absl::nullopt;
  }

  const ReferenceMap& reference_map_;
  RegexProgramBuilder regex_program_builder_;
};

}  // namespace

ProgramOptimizerFactory CreateRegexPrecompilationExtension(
    int regex_max_program_size) {
  return [=](PlannerContext& context, const AstImpl& ast) {
    return std::make_unique<RegexPrecompilationOptimization>(
        ast.reference_map(), regex_max_program_size);
  };
}
}  // namespace google::api::expr::runtime
