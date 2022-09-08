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

AttributeQualifier AttributeQualifier::Create(const CelValue& value) {
  switch (value.type()) {
    case Kind::kInt64:
      return AttributeQualifier(absl::in_place_type<int64_t>,
                                value.Int64OrDie());
    case Kind::kUint64:
      return AttributeQualifier(absl::in_place_type<uint64_t>,
                                value.Uint64OrDie());
    case Kind::kString:
      return AttributeQualifier(absl::in_place_type<std::string>,
                                std::string(value.StringOrDie().value()));
    case Kind::kBool:
      return AttributeQualifier(absl::in_place_type<bool>, value.BoolOrDie());
    default:
      return AttributeQualifier();
  }
}

bool AttributeQualifier::IsMatch(const CelValue& cel_value) const {
  return absl::visit(AttributeQualifierIsMatchVisitor{cel_value}, value_);
}

}  // namespace cel

namespace google::api::expr::runtime {

namespace {

// Visitation for attribute qualifier kinds
struct QualifierVisitor {
  CelAttributeQualifierPattern operator()(absl::string_view v) {
    if (v == "*") {
      return CelAttributeQualifierPattern::CreateWildcard();
    }
    return CelAttributeQualifierPattern::Create(CelValue::CreateStringView(v));
  }

  CelAttributeQualifierPattern operator()(int64_t v) {
    return CelAttributeQualifierPattern::Create(CelValue::CreateInt64(v));
  }

  CelAttributeQualifierPattern operator()(uint64_t v) {
    return CelAttributeQualifierPattern::Create(CelValue::CreateUint64(v));
  }

  CelAttributeQualifierPattern operator()(bool v) {
    return CelAttributeQualifierPattern::Create(CelValue::CreateBool(v));
  }

  CelAttributeQualifierPattern operator()(CelAttributeQualifierPattern v) {
    return v;
  }
};

}  // namespace

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
