#ifndef THIRD_PARTY_CEL_CPP_COMMON_ENUM_H_
#define THIRD_PARTY_CEL_CPP_COMMON_ENUM_H_

#include "common/type.h"
#include "internal/ref_countable.h"

namespace google {
namespace api {
namespace expr {
namespace common {

/**
 * A recognized named enum value.
 */
class NamedEnumValue final
    : public internal::Handle<const google::protobuf::EnumValueDescriptor*,
                              NamedEnumValue> {
 public:
  constexpr NamedEnumValue(const google::protobuf::EnumValueDescriptor* desc)
      : Handle(desc) {}

  inline int32_t value() const { return value_->number(); }
  inline EnumType type() const { return EnumType(value_->type()); }

  inline const std::string& ToString() const { return value_->full_name(); }
};

/**
 * An unnamed or unrecognized enum value.
 *
 * Constructed by 'EnumValue'.
 */
class UnnamedEnumValue final : public internal::RefCountable {
 public:
  ~UnnamedEnumValue() = default;

  inline int32_t value() const { return value_; }
  inline EnumType type() const { return type_; }

  std::string ToString() const;

  inline bool operator==(const UnnamedEnumValue& rhs) const {
    return value_ == rhs.value_ && type_ == rhs.type_;
  }
  inline bool operator!=(const UnnamedEnumValue& rhs) const {
    return value_ != rhs.value_ || type_ != rhs.type_;
  }

  std::size_t hash_code() const { return internal::Hash(value_, type_); }

 private:
  friend class EnumValue;
  friend class Value;

  constexpr UnnamedEnumValue(EnumType type, int32_t value)
      : type_(type), value_(value) {}

  EnumType type_;
  int32_t value_;
};

class EnumValue final {
 public:
  // Allow implicit conversion so visitors can overload using EnumValue or
  // the explicit class.
  constexpr EnumValue(NamedEnumValue value) : data_(value) {}
  EnumValue(UnnamedEnumValue value) : data_(value) {}
  EnumValue(EnumType type, int32_t value);

  inline bool is_named() const { return data_.index() == 0; }

  inline NamedEnumValue named_value() const { return absl::get<0>(data_); }

  inline const UnnamedEnumValue& unnamed_value() const {
    return absl::get<1>(data_);
  }

  int32_t value() const;
  EnumType type() const;

  std::string ToString() const;

 private:
  absl::variant<NamedEnumValue, UnnamedEnumValue> data_;
};

}  // namespace common
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_COMMON_ENUM_H_
