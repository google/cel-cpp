#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_H_

#include "google/protobuf/descriptor.h"
#include "absl/types/variant.h"
#include "internal/handle.h"

namespace google {
namespace api {
namespace expr {
namespace common {

/** The basic value types. */
enum class BasicTypeValue {
  kNull,
  kBool,
  kInt,
  kUint,
  kDouble,
  kString,
  kBytes,
  kType,
  kMap,
  kList,

  // Special value to require 'default' case in switch statements.
  DO_NOT_USE
};

class BasicType : public internal::Handle<BasicTypeValue, BasicType> {
 public:
  constexpr explicit BasicType(BasicTypeValue value) : Handle(value) {}

  inline const absl::string_view full_name() const { return ToString(); }

  inline bool operator==(BasicTypeValue value) const { return value_ == value; }
  inline bool operator!=(BasicTypeValue value) const { return value_ != value; }

  /**
   * Returns a canonical cel expression for the value.
   */
  const std::string& ToString() const;
};

/** An object type. */
class ObjectType
    : public internal::Handle<const google::protobuf::Descriptor*, ObjectType> {
 public:
  constexpr explicit ObjectType(const google::protobuf::Descriptor* desc)
      : Handle(desc) {}

  template <class T>
  constexpr static ObjectType For() {
    return ObjectType(T::descriptor());
  }

  inline absl::string_view full_name() const { return value_->full_name(); }

  /**
   * Returns a canonical cel expression for the value.
   */
  inline const std::string& ToString() const { return value_->full_name(); }

  std::unique_ptr<google::protobuf::Message> Unpack(const google::protobuf::Any& value);
};

/** An enum type. */
class EnumType
    : public internal::Handle<const google::protobuf::EnumDescriptor*, EnumType> {
 public:
  constexpr explicit EnumType(const google::protobuf::EnumDescriptor* desc)
      : Handle(desc) {}

  inline absl::string_view full_name() const { return value_->full_name(); }

  /**
   * Returns a canonical cel expression for the value.
   */
  inline const std::string& ToString() const { return value_->full_name(); }
};

/**
 * An unrecognized type.
 */
class UnrecognizedType final {
 public:
  explicit UnrecognizedType(absl::string_view full_name);

  absl::string_view full_name() const;
  inline std::size_t hash_code() const { return hash_code_; }

  inline bool operator==(const UnrecognizedType& rhs) const {
    return hash_code_ == rhs.hash_code_ && full_name() == rhs.full_name();
  }

  inline bool operator!=(const UnrecognizedType& rhs) const {
    return !(*this == rhs);
  }

  /**
   * Returns a canonical cel expression for the value.
   */
  inline const std::string& ToString() const { return string_rep_; }

 private:
  std::string string_rep_;
  std::size_t hash_code_;
};

/** A type value. */
class Type final {
 public:
  // Allow for implicit conversion, so visitors can implement overloads
  // for either Type or more specific instances.
  constexpr Type(BasicType basic_type) : data_(basic_type) {}
  constexpr Type(EnumType enum_type) : data_(enum_type) {}
  constexpr Type(ObjectType object_type) : data_(object_type) {}
  explicit Type(const std::string& full_name);
  explicit Type(absl::string_view full_name) : Type(std::string(full_name)) {}
  explicit Type(const char* full_name) : Type(std::string(full_name)) {}

  Type(const UnrecognizedType& unrecognized_type) : data_(unrecognized_type) {}
  Type(UnrecognizedType&& unrecognized_type)
      : data_(std::move(unrecognized_type)) {}

  absl::string_view full_name() const;

  bool is_basic() const { return absl::holds_alternative<BasicType>(data_); }
  bool is_enum() const { return absl::holds_alternative<EnumType>(data_); }
  bool is_object() const { return absl::holds_alternative<ObjectType>(data_); }
  bool is_unrecognized() const {
    return absl::holds_alternative<UnrecognizedType>(data_);
  }

  inline BasicType basic_type() const { return absl::get<BasicType>(data_); }
  inline EnumType enum_type() const { return absl::get<EnumType>(data_); }
  inline ObjectType object_type() const { return absl::get<ObjectType>(data_); }

  inline bool operator==(const Type& rhs) const { return data_ == rhs.data_; }
  inline bool operator!=(const Type& rhs) const { return data_ != rhs.data_; }

  /** The hash code for this value. */
  inline std::size_t hash_code() const { return internal::Hash(data_); }

  /**
   * Returns a canonical cel expression for the value.
   */
  const std::string& ToString() const;

 private:
  absl::variant<BasicType, EnumType, ObjectType, UnrecognizedType> data_;
};

inline std::ostream& operator<<(std::ostream& os, const Type& value) {
  return os << value.ToString();
}

}  // namespace common
}  // namespace expr
}  // namespace api
}  // namespace google

namespace std {

template <>
struct hash<google::api::expr::common::UnrecognizedType>
    : google::api::expr::internal::Hasher {};

template <>
struct hash<google::api::expr::common::BasicType>
    : google::api::expr::internal::Hasher {};

template <>
struct hash<google::api::expr::common::EnumType>
    : google::api::expr::common::EnumType::Hasher {};

template <>
struct hash<google::api::expr::common::ObjectType>
    : google::api::expr::common::ObjectType::Hasher {};

}  // namespace std

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_H_
