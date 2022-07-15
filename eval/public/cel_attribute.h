#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_ATTRIBUTE_PATTERN_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_ATTRIBUTE_PATTERN_H_

#include <sys/types.h>

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "eval/public/cel_value.h"
#include "eval/public/cel_value_internal.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

// CelAttributeQualifier represents a segment in
// attribute resolutuion path. A segment can be qualified by values of
// following types: string/int64_t/uint64/bool.
class CelAttributeQualifier {
 public:
  // Factory method.
  static CelAttributeQualifier Create(CelValue value);

  CelAttributeQualifier(const CelAttributeQualifier&) = default;
  CelAttributeQualifier(CelAttributeQualifier&&) = default;

  CelAttributeQualifier& operator=(const CelAttributeQualifier&) = default;
  CelAttributeQualifier& operator=(CelAttributeQualifier&&) = default;

  CelValue::Type type() const;

  // Family of Get... methods. Return values if requested type matches the
  // stored one.
  std::optional<int64_t> GetInt64Key() const {
    return std::holds_alternative<int64_t>(value_)
               ? std::optional<int64_t>(std::get<1>(value_))
               : std::nullopt;
  }

  std::optional<uint64_t> GetUint64Key() const {
    return std::holds_alternative<uint64_t>(value_)
               ? std::optional<uint64_t>(std::get<2>(value_))
               : std::nullopt;
  }

  std::optional<absl::string_view> GetStringKey() const {
    return std::holds_alternative<std::string>(value_)
               ? std::optional<absl::string_view>(std::get<3>(value_))
               : std::nullopt;
  }

  std::optional<bool> GetBoolKey() const {
    return std::holds_alternative<bool>(value_)
               ? std::optional<bool>(std::get<4>(value_))
               : std::nullopt;
  }

  bool operator==(const CelAttributeQualifier& other) const {
    return IsMatch(other);
  }

  bool IsMatch(const CelValue& cel_value) const;

  bool IsMatch(absl::string_view other_key) const {
    std::optional<absl::string_view> key = GetStringKey();
    return (key.has_value() && key.value() == other_key);
  }

 private:
  friend class CelAttribute;

  CelAttributeQualifier() = default;

  template <typename T>
  CelAttributeQualifier(std::in_place_type_t<T> in_place_type, T&& value)
      : value_(in_place_type, std::forward<T>(value)) {}

  bool IsMatch(const CelAttributeQualifier& other) const;

  // The previous implementation of CelAttribute preserved all CelValue
  // instances, regardless of whether they are supported in this context or not.
  // We represented unsupported types by using the first alternative and thus
  // preserve backwards compatibility with the result of `type()` above.
  std::variant<CelValue::Type, int64_t, uint64_t, std::string, bool> value_;
};

// CelAttributeQualifierPattern matches a segment in
// attribute resolutuion path. CelAttributeQualifierPattern is capable of
// matching path elements of types string/int64_t/uint64/bool.
class CelAttributeQualifierPattern {
 private:
  // Qualifier value. If not set, treated as wildcard.
  std::optional<CelAttributeQualifier> value_;

  explicit CelAttributeQualifierPattern(
      std::optional<CelAttributeQualifier> value)
      : value_(std::move(value)) {}

 public:
  // Factory method.
  static CelAttributeQualifierPattern Create(CelValue value) {
    return CelAttributeQualifierPattern(CelAttributeQualifier::Create(value));
  }

  static CelAttributeQualifierPattern CreateWildcard() {
    return CelAttributeQualifierPattern(std::nullopt);
  }

  bool IsWildcard() const { return !value_.has_value(); }

  bool IsMatch(const CelAttributeQualifier& qualifier) const {
    if (IsWildcard()) return true;
    return value_.value() == qualifier;
  }

  bool IsMatch(const CelValue& cel_value) const {
    if (!value_.has_value()) {
      switch (cel_value.type()) {
        case CelValue::Type::kInt64:
        case CelValue::Type::kUint64:
        case CelValue::Type::kString:
        case CelValue::Type::kBool: {
          return true;
        }
        default: {
          return false;
        }
      }
    }
    return value_->IsMatch(cel_value);
  }

  bool IsMatch(absl::string_view other_key) const {
    if (!value_.has_value()) return true;
    return value_->IsMatch(other_key);
  }
};

// CelAttribute represents resolved attribute path.
class CelAttribute {
 public:
  CelAttribute(std::string variable_name,
               std::vector<CelAttributeQualifier> qualifier_path)
      : variable_name_(std::move(variable_name)),
        qualifier_path_(std::move(qualifier_path)) {}

  CelAttribute(const google::api::expr::v1alpha1::Expr& variable,
               std::vector<CelAttributeQualifier> qualifier_path)
      : CelAttribute(variable.ident_expr().name(), std::move(qualifier_path)) {}

  absl::string_view variable_name() const { return variable_name_; }

  bool has_variable_name() const { return !variable_name_.empty(); }

  const std::vector<CelAttributeQualifier>& qualifier_path() const {
    return qualifier_path_;
  }

  bool operator==(const CelAttribute& other) const;

  const absl::StatusOr<std::string> AsString() const;

 private:
  std::string variable_name_;
  std::vector<CelAttributeQualifier> qualifier_path_;
};

// CelAttributePattern is a fully-qualified absolute attribute path pattern.
// Supported segments steps in the path are:
// - field selection;
// - map lookup by key;
// - list access by index.
class CelAttributePattern {
 public:
  // MatchType enum specifies how closely pattern is matching the attribute:
  enum class MatchType {
    NONE,     // Pattern does not match attribute itself nor its children
    PARTIAL,  // Pattern matches an entity nested within attribute;
    FULL      // Pattern matches an attribute itself.
  };

  CelAttributePattern(std::string variable,
                      std::vector<CelAttributeQualifierPattern> qualifier_path)
      : variable_(std::move(variable)),
        qualifier_path_(std::move(qualifier_path)) {}

  absl::string_view variable() const { return variable_; }

  const std::vector<CelAttributeQualifierPattern>& qualifier_path() const {
    return qualifier_path_;
  }

  // Matches the pattern to an attribute.
  // Distinguishes between no-match, partial match and full match cases.
  MatchType IsMatch(const CelAttribute& attribute) const {
    MatchType result = MatchType::NONE;
    if (attribute.variable_name() != variable_) {
      return result;
    }

    auto max_index = qualifier_path().size();
    result = MatchType::FULL;
    if (qualifier_path().size() > attribute.qualifier_path().size()) {
      max_index = attribute.qualifier_path().size();
      result = MatchType::PARTIAL;
    }

    for (size_t i = 0; i < max_index; i++) {
      if (!(qualifier_path()[i].IsMatch(attribute.qualifier_path()[i]))) {
        return MatchType::NONE;
      }
    }
    return result;
  }

 private:
  std::string variable_;
  std::vector<CelAttributeQualifierPattern> qualifier_path_;
};

// Short-hand helper for creating |CelAttributePattern|s. string_view arguments
// must outlive the returned pattern.
CelAttributePattern CreateCelAttributePattern(
    absl::string_view variable,
    std::initializer_list<std::variant<absl::string_view, int64_t, uint64_t, bool,
                                       CelAttributeQualifierPattern>>
        path_spec = {});

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_ATTRIBUTE_PATTERN_H_
