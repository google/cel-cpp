#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_ATTRIBUTE_PATTERN_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_ATTRIBUTE_PATTERN_H_

#include <sys/types.h>

#include <algorithm>
#include <cstddef>
#include <initializer_list>

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
  static CelAttributeQualifier Create(CelValue value) {
    return CelAttributeQualifier(value);
  }

  CelValue::Type type() const { return value_.type(); }

  // Family of Get... methods. Return values if requested type matches the
  // stored one.
  absl::optional<int64_t> GetInt64Key() const {
    return (value_.IsInt64()) ? absl::optional<int64_t>(value_.Int64OrDie())
                              : absl::nullopt;
  }

  absl::optional<uint64_t> GetUint64Key() const {
    return (value_.IsUint64()) ? absl::optional<uint64_t>(value_.Uint64OrDie())
                               : absl::nullopt;
  }

  absl::optional<absl::string_view> GetStringKey() const {
    return (value_.IsString())
               ? absl::optional<absl::string_view>(value_.StringOrDie().value())
               : absl::nullopt;
  }

  absl::optional<bool> GetBoolKey() const {
    return (value_.IsBool()) ? absl::optional<bool>(value_.BoolOrDie())
                             : absl::nullopt;
  }

  bool operator==(const CelAttributeQualifier& other) const {
    return IsMatch(other.value_);
  }

  bool IsMatch(const CelValue& cel_value) const;

  bool IsMatch(absl::string_view other_key) const {
    absl::optional<absl::string_view> key = GetStringKey();
    return (key.has_value() && key.value() == other_key);
  }

 private:
  friend class CelAttribute;
  explicit CelAttributeQualifier(CelValue value) : value_(value) {}

  template <typename T, typename Op>
  T Visit(Op&& operation) const {
    return value_.InternalVisit<T>(operation);
  }

  CelValue value_;
};

// CelAttributeQualifierPattern matches a segment in
// attribute resolutuion path. CelAttributeQualifierPattern is capable of
// matching path elements of types string/int64_t/uint64/bool.
class CelAttributeQualifierPattern {
 private:
  // Qualifier value. If not set, treated as wildcard.
  absl::optional<CelAttributeQualifier> value_;

  explicit CelAttributeQualifierPattern(
      absl::optional<CelAttributeQualifier> value)
      : value_(value) {}

 public:
  // Factory method.
  static CelAttributeQualifierPattern Create(CelValue value) {
    return CelAttributeQualifierPattern(CelAttributeQualifier::Create(value));
  }

  static CelAttributeQualifierPattern CreateWildcard() {
    return CelAttributeQualifierPattern(absl::nullopt);
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
  CelAttribute(google::api::expr::v1alpha1::Expr variable,
               std::vector<CelAttributeQualifier> qualifier_path)
      : variable_(std::move(variable)),
        qualifier_path_(std::move(qualifier_path)) {}

  const google::api::expr::v1alpha1::Expr& variable() const { return variable_; }

  const std::vector<CelAttributeQualifier>& qualifier_path() const {
    return qualifier_path_;
  }

  bool operator==(const CelAttribute& other) const;

  const absl::StatusOr<std::string> AsString() const;

 private:
  google::api::expr::v1alpha1::Expr variable_;
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
    if (attribute.variable().ident_expr().name() != variable_) {
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
    std::initializer_list<absl::variant<absl::string_view, int64_t, uint64_t, bool,
                                        CelAttributeQualifierPattern>>
        path_spec = {});

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_ATTRIBUTE_PATTERN_H_
