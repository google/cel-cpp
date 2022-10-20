#include "eval/public/cel_attribute.h"

#include <algorithm>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "eval/public/cel_value.h"

namespace cel {
namespace {

using ::google::api::expr::runtime::CelValue;

struct AttributeQualifierIsMatchVisitor final {
  const CelValue& value;

  bool operator()(const Kind& ignored) const {
    static_cast<void>(ignored);
    return false;
  }

  bool operator()(int64_t other) const {
    int64_t value_value;
    return value.GetValue(&value_value) && value_value == other;
  }

  bool operator()(uint64_t other) const {
    uint64_t value_value;
    return value.GetValue(&value_value) && value_value == other;
  }

  bool operator()(const std::string& other) const {
    CelValue::StringHolder value_value;
    return value.GetValue(&value_value) && value_value.value() == other;
  }

  bool operator()(bool other) const {
    bool value_value;
    return value.GetValue(&value_value) && value_value == other;
  }
};

}  // namespace

Attribute::Attribute(const google::api::expr::v1alpha1::Expr& variable,
                     std::vector<AttributeQualifier> qualifier_path)
    : Attribute(variable.ident_expr().name(), std::move(qualifier_path)) {}

}  // namespace cel

namespace google::api::expr::runtime {

namespace {

// Visitation for attribute qualifier kinds
struct QualifierVisitor {
  CelAttributeQualifierPattern operator()(absl::string_view v) {
    if (v == "*") {
      return CelAttributeQualifierPattern::CreateWildcard();
    }
    return CelAttributeQualifierPattern::OfString(std::string(v));
  }

  CelAttributeQualifierPattern operator()(int64_t v) {
    return CelAttributeQualifierPattern::OfInt(v);
  }

  CelAttributeQualifierPattern operator()(uint64_t v) {
    return CelAttributeQualifierPattern::OfUint(v);
  }

  CelAttributeQualifierPattern operator()(bool v) {
    return CelAttributeQualifierPattern::OfBool(v);
  }

  CelAttributeQualifierPattern operator()(CelAttributeQualifierPattern v) {
    return v;
  }
};

}  // namespace

CelAttributeQualifierPattern CreateCelAttributeQualifierPattern(
    const CelValue& value) {
  switch (value.type()) {
    case cel::Kind::kInt64:
      return CelAttributeQualifierPattern::OfInt(value.Int64OrDie());
    case cel::Kind::kUint64:
      return CelAttributeQualifierPattern::OfUint(value.Uint64OrDie());
    case cel::Kind::kString:
      return CelAttributeQualifierPattern::OfString(
          std::string(value.StringOrDie().value()));
    case cel::Kind::kBool:
      return CelAttributeQualifierPattern::OfBool(value.BoolOrDie());
    default:
      return CelAttributeQualifierPattern(CelAttributeQualifier());
  }
}

CelAttributeQualifier CreateCelAttributeQualifier(const CelValue& value) {
  switch (value.type()) {
    case cel::Kind::kInt64:
      return CelAttributeQualifier::OfInt(value.Int64OrDie());
    case cel::Kind::kUint64:
      return CelAttributeQualifier::OfUint(value.Uint64OrDie());
    case cel::Kind::kString:
      return CelAttributeQualifier::OfString(
          std::string(value.StringOrDie().value()));
    case cel::Kind::kBool:
      return CelAttributeQualifier::OfBool(value.BoolOrDie());
    default:
      return CelAttributeQualifier();
  }
}

CelAttributePattern CreateCelAttributePattern(
    absl::string_view variable,
    std::initializer_list<absl::variant<absl::string_view, int64_t, uint64_t, bool,
                                        CelAttributeQualifierPattern>>
        path_spec) {
  std::vector<CelAttributeQualifierPattern> path;
  path.reserve(path_spec.size());
  for (const auto& spec_elem : path_spec) {
    path.emplace_back(absl::visit(QualifierVisitor(), spec_elem));
  }
  return CelAttributePattern(std::string(variable), std::move(path));
}

}  // namespace google::api::expr::runtime
