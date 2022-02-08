#include "eval/public/cel_attribute.h"

#include <algorithm>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {
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

// Visitor for appending string representation for different qualifier kinds.
class CelAttributeStringPrinter {
 public:
  // String representation for the given qualifier is appended to output.
  // output must be non-null.
  explicit CelAttributeStringPrinter(std::string* output) : output_(*output) {}

  absl::Status operator()(int64_t index) {
    absl::StrAppend(&output_, "[", index, "]");
    return absl::OkStatus();
  }

  absl::Status operator()(uint64_t index) {
    absl::StrAppend(&output_, "[", index, "]");
    return absl::OkStatus();
  }

  absl::Status operator()(bool bool_key) {
    absl::StrAppend(&output_, "[", (bool_key) ? "true" : "false", "]");
    return absl::OkStatus();
  }

  absl::Status operator()(const CelValue::StringHolder& field) {
    absl::StrAppend(&output_, ".", field.value());
    return absl::OkStatus();
  }

  template <typename T>
  absl::Status operator()(const T&) {
    // Attributes are represented as generic CelValues, but remaining kinds are
    // not legal attribute qualifiers.
    return absl::InvalidArgumentError(absl::StrCat(
        "Unsupported attribute qualifier ",
        CelValue::TypeName(CelValue::Type(CelValue::IndexOf<T>::value))));
  }

 private:
  std::string& output_;
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

bool CelAttribute::operator==(const CelAttribute& other) const {
  // TODO(issues/41) we only support Ident-rooted attributes at the moment.
  if (!variable().has_ident_expr() || !other.variable().has_ident_expr()) {
    return false;
  }

  if (variable().ident_expr().name() != other.variable().ident_expr().name()) {
    return false;
  }

  if (qualifier_path().size() != other.qualifier_path().size()) {
    return false;
  }

  for (size_t i = 0; i < qualifier_path().size(); i++) {
    if (!(qualifier_path()[i] == other.qualifier_path()[i])) {
      return false;
    }
  }

  return true;
}

const absl::StatusOr<std::string> CelAttribute::AsString() const {
  if (variable_.ident_expr().name().empty()) {
    return absl::InvalidArgumentError(
        "Only ident rooted attributes are supported.");
  }

  std::string result = variable_.ident_expr().name();

  for (const auto& qualifier : qualifier_path_) {
    CEL_RETURN_IF_ERROR(
        qualifier.Visit<absl::Status>(CelAttributeStringPrinter(&result)));
  }

  return result;
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
