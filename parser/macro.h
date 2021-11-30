// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_PARSER_MACRO_H_
#define THIRD_PARTY_CEL_CPP_PARSER_MACRO_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/base/attributes.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace google::api::expr::parser {
class SourceFactory;
}

namespace cel {

using SourceFactory = google::api::expr::parser::SourceFactory;

// MacroExpander converts the target and args of a function call that matches a
// Macro.
//
// Note: when the Macros.IsReceiverStyle() is true, the target argument will
// be Expr::default_instance().
using MacroExpander = std::function<google::api::expr::v1alpha1::Expr(
    const std::shared_ptr<SourceFactory>& sf, int64_t macro_id,
    const google::api::expr::v1alpha1::Expr&,
    // This should be absl::Span instead of std::vector.
    const std::vector<google::api::expr::v1alpha1::Expr>&)>;

// Macro interface for describing the function signature to match and the
// MacroExpander to apply.
//
// Note: when a Macro should apply to multiple overloads (based on arg count) of
// a given function, a Macro should be created per arg-count.
class Macro final {
 public:
  static absl::StatusOr<Macro> Global(absl::string_view name,
                                      size_t argument_count,
                                      MacroExpander expander);

  static absl::StatusOr<Macro> GlobalVarArg(absl::string_view name,
                                            MacroExpander expander);

  static absl::StatusOr<Macro> Receiver(absl::string_view name,
                                        size_t argument_count,
                                        MacroExpander expander);

  static absl::StatusOr<Macro> ReceiverVarArg(absl::string_view name,
                                              MacroExpander expander);

  // Create a Macro for a global function with the specified number of arguments
  ABSL_DEPRECATED("Use static factory methods instead.")
  Macro(absl::string_view function, size_t arg_count, MacroExpander expander,
        bool receiver_style = false)
      : key_(absl::StrCat(function, ":", arg_count, ":",
                          receiver_style ? "true" : "false")),
        arg_count_(arg_count),
        expander_(std::make_shared<MacroExpander>(std::move(expander))),
        receiver_style_(receiver_style),
        var_arg_style_(false) {}

  ABSL_DEPRECATED("Use static factory methods instead.")
  Macro(absl::string_view function, MacroExpander expander,
        bool receiver_style = false)
      : key_(absl::StrCat(function, ":*:", receiver_style ? "true" : "false")),
        arg_count_(0),
        expander_(std::make_shared<MacroExpander>(std::move(expander))),
        receiver_style_(receiver_style),
        var_arg_style_(true) {}

  // Function name to match.
  absl::string_view function() const { return key().substr(0, key_.find(':')); }

  ABSL_DEPRECATED("Use argument_count() instead.")
  int argCount() const { return static_cast<int>(argument_count()); }

  // argument_count() for the function call.
  //
  // When the macro is a var-arg style macro, the return value will be zero, but
  // the MacroKey will contain a `*` where the arg count would have been.
  size_t argument_count() const { return arg_count_; }

  ABSL_DEPRECATED("Use is_receiver_style() instead.")
  bool isReceiverStyle() const { return receiver_style_; }

  // IsReceiverStyle returns true if the macro matches a receiver style call.
  bool is_receiver_style() const { return receiver_style_; }

  bool is_variadic() const { return var_arg_style_; }

  ABSL_DEPRECATED("Use key() instead.")
  std::string macroKey() const { return key_; }

  // key() returns the macro signatures accepted by this macro.
  //
  // Format: `<function>:<arg-count>:<is-receiver>`.
  //
  // When the macros is a var-arg style macro, the `arg-count` value is
  // represented as a `*`.
  absl::string_view key() const { return key_; }

  // Expander returns the MacroExpander to apply when the macro key matches the
  // parsed call signature.
  const MacroExpander& expander() const { return *expander_; }

  ABSL_DEPRECATED("Use Expand() instead.")
  google::api::expr::v1alpha1::Expr expand(
      const std::shared_ptr<SourceFactory>& sf, int64_t macro_id,
      const google::api::expr::v1alpha1::Expr& target,
      const std::vector<google::api::expr::v1alpha1::Expr>& args) {
    return Expand(sf, macro_id, target, args);
  }

  google::api::expr::v1alpha1::Expr Expand(
      const std::shared_ptr<SourceFactory>& sf, int64_t macro_id,
      const google::api::expr::v1alpha1::Expr& target,
      const std::vector<google::api::expr::v1alpha1::Expr>& args) const {
    return (expander())(sf, macro_id, target, args);
  }

  static std::vector<Macro> AllMacros();

 private:
  std::string key_;
  size_t arg_count_;
  std::shared_ptr<MacroExpander> expander_;
  bool receiver_style_;
  bool var_arg_style_;
};

}  // namespace cel

namespace google {
namespace api {
namespace expr {
namespace parser {

using MacroExpander = cel::MacroExpander;

using Macro = cel::Macro;

}  // namespace parser
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_PARSER_MACRO_H_
