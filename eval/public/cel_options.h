#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_OPTIONS_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_OPTIONS_H_

#include "google/protobuf/arena.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Options for unknown processing.
enum class UnknownProcessingOptions {
  // No unknown processing.
  kDisabled,
  // Only attributes supported.
  kAttributeOnly,
  // Attributes and functions supported. Function results are dependent on the
  // logic for handling unknown_attributes, so clients must opt in to both.
  kAttributeAndFunction
};

// Interpreter options for controlling evaluation and builtin functions.
struct InterpreterOptions {
  // Level of unknown support enabled.
  UnknownProcessingOptions unknown_processing =
      UnknownProcessingOptions::kDisabled;

  bool enable_missing_attribute_errors = false;

  // Enable functions which return the string.size() as the number of unicode
  // codepoints.
  bool enable_string_size_as_unicode_codepoints = true;

  // Enable short-circuiting of the logical operator evaluation. If enabled,
  // AND, OR, and TERNARY do not evaluate the entire expression once the the
  // resulting value is known from the left-hand side.
  bool short_circuiting = true;

  // DEPRECATED. This option has no effect.
  bool partial_string_match = true;

  // Enable constant folding during the expression creation. If enabled,
  // an arena must be provided for constant generation.
  // Note that expression tracing applies a modified expression if this option
  // is enabled.
  bool constant_folding = false;
  google::protobuf::Arena* constant_arena = nullptr;

  // Enable comprehension expressions (e.g. exists, all)
  bool enable_comprehension = true;

  // Set maximum number of iterations in the comprehension expressions if
  // comprehensions are enabled. The limit applies globally per an evaluation,
  // including the nested loops as well. Use value 0 to disable the upper bound.
  int comprehension_max_iterations = 0;

  // Enable RE2 match() overload.
  bool enable_regex = true;

  // Set maximum program size for RE2 regex if regex overload is enabled.
  // Evaluates to an error if a regex exceeds it. Use value 0 to disable the
  // upper bound.
  int regex_max_program_size = 0;

  // Enable string() overloads.
  bool enable_string_conversion = true;

  // Enable string concatenation overload.
  bool enable_string_concat = true;

  // Enable list concatenation overload.
  bool enable_list_concat = true;

  // Enable list membership overload.
  bool enable_list_contains = true;

  // Treat builder warnings as fatal errors.
  bool fail_on_warnings = true;

  // Enable the resolution of qualified type identifiers as type values instead
  // of field selections.
  //
  // This toggle may cause certain identifiers which overlap with CEL built-in
  // type or with protobuf message types linked into the binary to be resolved
  // as static type values rather than as per-eval variables.
  bool enable_qualified_type_identifiers = false;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_OPTIONS_H_
