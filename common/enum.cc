#include "common/enum.h"

#include "internal/cel_printer.h"

namespace google {
namespace api {
namespace expr {
namespace common {

namespace {
using ::google::api::expr::internal::ToCallString;

struct ValueVisitor {
  template <typename T>
  int32_t operator()(const T& value) {
    return value.value();
  }
};

struct TypeVisitor {
  template <typename T>
  EnumType operator()(const T& value) {
    return value.type();
  }
};

struct ToStringVisitor {
  template <typename T>
  std::string operator()(const T& value) {
    return value.ToString();
  }
};

}  // namespace

std::string UnnamedEnumValue::ToString() const {
  return internal::ToCallString(type_.value()->full_name(), value_);
}

EnumValue::EnumValue(EnumType type, int32_t value)
    : data_(NamedEnumValue(type.value()->FindValueByNumber(value))) {
  if (absl::get<NamedEnumValue>(data_).Handle::value() == nullptr) {
    data_ = UnnamedEnumValue(type, value);
  }
}

int32_t EnumValue::value() const { return absl::visit(ValueVisitor(), data_); }

EnumType EnumValue::type() const { return absl::visit(TypeVisitor(), data_); }

std::string EnumValue::ToString() const {
  return absl::visit(ToStringVisitor(), data_);
}

}  // namespace common
}  // namespace expr
}  // namespace api
}  // namespace google
