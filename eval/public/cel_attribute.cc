#include "eval/public/cel_attribute.h"

#include <algorithm>
#include <string>
#include <variant>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "eval/public/cel_value.h"

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

// Visitor for appending string representation for different qualifier kinds.
class CelAttributeStringPrinter {
 public:
  // String representation for the given qualifier is appended to output.
  // output must be non-null.
  explicit CelAttributeStringPrinter(std::string* output, CelValue::Type type)
      : output_(*output), type_(type) {}

  absl::Status operator()(const CelValue::Type& ignored) const {
    // Attributes are represented as a variant, with illegal attribute
    // qualifiers represented with their type as the first alternative.
    return absl::InvalidArgumentError(absl::StrCat(
        "Unsupported attribute qualifier ", CelValue::TypeName(type_)));
  }

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

  absl::Status operator()(const std::string& field) {
    absl::StrAppend(&output_, ".", field);
    return absl::OkStatus();
  }

 private:
  std::string& output_;
  CelValue::Type type_;
};

struct CelAttributeQualifierTypeVisitor final {
  CelValue::Type operator()(const CelValue::Type& type) const { return type; }

  CelValue::Type operator()(int64_t ignored) const {
    static_cast<void>(ignored);
    return CelValue::Type::kInt64;
  }

  CelValue::Type operator()(uint64_t ignored) const {
    static_cast<void>(ignored);
    return CelValue::Type::kUint64;
  }

  CelValue::Type operator()(const std::string& ignored) const {
    static_cast<void>(ignored);
    return CelValue::Type::kString;
  }

  CelValue::Type operator()(bool ignored) const {
    static_cast<void>(ignored);
    return CelValue::Type::kBool;
  }
};

struct CelAttributeQualifierIsMatchVisitor final {
  const CelValue& value;

  bool operator()(const CelValue::Type& ignored) const {
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

CelValue::Type CelAttributeQualifier::type() const {
  return std::visit(CelAttributeQualifierTypeVisitor{}, value_);
}

CelAttributeQualifier CelAttributeQualifier::Create(CelValue value) {
  switch (value.type()) {
    case CelValue::Type::kInt64:
      return CelAttributeQualifier(std::in_place_type<int64_t>,
                                   value.Int64OrDie());
    case CelValue::Type::kUint64:
      return CelAttributeQualifier(std::in_place_type<uint64_t>,
                                   value.Uint64OrDie());
    case CelValue::Type::kString:
      return CelAttributeQualifier(std::in_place_type<std::string>,
                                   std::string(value.StringOrDie().value()));
    case CelValue::Type::kBool:
      return CelAttributeQualifier(std::in_place_type<bool>, value.BoolOrDie());
    default:
      return CelAttributeQualifier();
  }
}

CelAttributePattern CreateCelAttributePattern(
    absl::string_view variable,
    std::initializer_list<std::variant<absl::string_view, int64_t, uint64_t, bool,
                                       CelAttributeQualifierPattern>>
        path_spec) {
  std::vector<CelAttributeQualifierPattern> path;
  path.reserve(path_spec.size());
  for (const auto& spec_elem : path_spec) {
    path.emplace_back(std::visit(QualifierVisitor(), spec_elem));
  }
  return CelAttributePattern(std::string(variable), std::move(path));
}

bool CelAttribute::operator==(const CelAttribute& other) const {
  // TODO(issues/41) we only support Ident-rooted attributes at the moment.
  if (variable_name() != other.variable_name()) {
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
  if (variable_name().empty()) {
    return absl::InvalidArgumentError(
        "Only ident rooted attributes are supported.");
  }

  std::string result = std::string(variable_name());

  for (const auto& qualifier : qualifier_path_) {
    CEL_RETURN_IF_ERROR(
        std::visit(CelAttributeStringPrinter(&result, qualifier.type()),
                   qualifier.value_));
  }

  return result;
}

bool CelAttributeQualifier::IsMatch(const CelValue& cel_value) const {
  return std::visit(CelAttributeQualifierIsMatchVisitor{cel_value}, value_);
}

bool CelAttributeQualifier::IsMatch(const CelAttributeQualifier& other) const {
  if (std::holds_alternative<CelValue::Type>(value_) ||
      std::holds_alternative<CelValue::Type>(other.value_)) {
    return false;
  }
  return value_ == other.value_;
}

}  // namespace google::api::expr::runtime
