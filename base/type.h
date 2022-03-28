// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_BASE_TYPE_H_
#define THIRD_PARTY_CEL_CPP_BASE_TYPE_H_

#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/hash/hash.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "base/handle.h"
#include "base/internal/type.pre.h"  // IWYU pragma: export
#include "base/kind.h"
#include "base/memory_manager.h"
#include "internal/casts.h"
#include "internal/rtti.h"

namespace cel {

class Type;
class NullType;
class ErrorType;
class DynType;
class AnyType;
class BoolType;
class IntType;
class UintType;
class DoubleType;
class StringType;
class BytesType;
class DurationType;
class TimestampType;
class EnumType;
class ListType;
class MapType;
class TypeFactory;
class TypeProvider;
class TypeManager;

class NullValue;
class ErrorValue;
class BoolValue;
class IntValue;
class UintValue;
class DoubleValue;
class BytesValue;
class StringValue;
class DurationValue;
class TimestampValue;
class EnumValue;
class ValueFactory;

namespace internal {
template <typename T>
class NoDestructor;
}

// A representation of a CEL type that enables reflection, for static analysis,
// and introspection, for program construction, of types.
class Type : public base_internal::Resource {
 public:
  // Returns the type kind.
  virtual Kind kind() const = 0;

  // Returns the type name, i.e. "list".
  virtual absl::string_view name() const = 0;

  // Returns the type parameters of the type, i.e. key and value type of map.
  virtual absl::Span<const Transient<const Type>> parameters() const;

  virtual std::string DebugString() const;

 private:
  friend class NullType;
  friend class ErrorType;
  friend class DynType;
  friend class AnyType;
  friend class BoolType;
  friend class IntType;
  friend class UintType;
  friend class DoubleType;
  friend class StringType;
  friend class BytesType;
  friend class DurationType;
  friend class TimestampType;
  friend class EnumType;
  friend class StructType;
  friend class ListType;
  friend class MapType;
  friend class base_internal::TypeHandleBase;

  Type() = default;
  Type(const Type&) = default;
  Type(Type&&) = default;

  // Called by base_internal::TypeHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Type& type) { return true; }

  // For non-inlined types that are reference counted, this is the result of
  // `sizeof` and `alignof` for the most derived class.
  std::pair<size_t, size_t> SizeAndAlignment() const override;

  using base_internal::Resource::Ref;
  using base_internal::Resource::Unref;

  // Called by base_internal::TypeHandleBase.
  virtual bool Equals(const Type& other) const;

  // Called by base_internal::TypeHandleBase.
  virtual void HashValue(absl::HashState state) const;
};

class NullType final : public Type {
 public:
  Kind kind() const override { return Kind::kNullType; }

  absl::string_view name() const override { return "null_type"; }

 private:
  friend class NullValue;
  friend class TypeFactory;
  template <typename T>
  friend class internal::NoDestructor;
  friend class base_internal::TypeHandleBase;

  // Called by base_internal::TypeHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Type& type) { return type.kind() == Kind::kNullType; }

  ABSL_ATTRIBUTE_PURE_FUNCTION static const NullType& Get();

  NullType() = default;

  NullType(const NullType&) = delete;
  NullType(NullType&&) = delete;
};

class ErrorType final : public Type {
 public:
  Kind kind() const override { return Kind::kError; }

  absl::string_view name() const override { return "*error*"; }

 private:
  friend class ErrorValue;
  friend class TypeFactory;
  template <typename T>
  friend class internal::NoDestructor;
  friend class base_internal::TypeHandleBase;

  // Called by base_internal::TypeHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Type& type) { return type.kind() == Kind::kError; }

  ABSL_ATTRIBUTE_PURE_FUNCTION static const ErrorType& Get();

  ErrorType() = default;

  ErrorType(const ErrorType&) = delete;
  ErrorType(ErrorType&&) = delete;
};

class DynType final : public Type {
 public:
  Kind kind() const override { return Kind::kDyn; }

  absl::string_view name() const override { return "dyn"; }

 private:
  friend class TypeFactory;
  template <typename T>
  friend class internal::NoDestructor;
  friend class base_internal::TypeHandleBase;

  // Called by base_internal::TypeHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Type& type) { return type.kind() == Kind::kDyn; }

  ABSL_ATTRIBUTE_PURE_FUNCTION static const DynType& Get();

  DynType() = default;

  DynType(const DynType&) = delete;
  DynType(DynType&&) = delete;
};

class AnyType final : public Type {
 public:
  Kind kind() const override { return Kind::kAny; }

  absl::string_view name() const override { return "google.protobuf.Any"; }

 private:
  friend class TypeFactory;
  template <typename T>
  friend class internal::NoDestructor;
  friend class base_internal::TypeHandleBase;

  // Called by base_internal::TypeHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Type& type) { return type.kind() == Kind::kAny; }

  ABSL_ATTRIBUTE_PURE_FUNCTION static const AnyType& Get();

  AnyType() = default;

  AnyType(const AnyType&) = delete;
  AnyType(AnyType&&) = delete;
};

class BoolType final : public Type {
 public:
  Kind kind() const override { return Kind::kBool; }

  absl::string_view name() const override { return "bool"; }

 private:
  friend class BoolValue;
  friend class TypeFactory;
  template <typename T>
  friend class internal::NoDestructor;
  friend class base_internal::TypeHandleBase;

  // Called by base_internal::TypeHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Type& type) { return type.kind() == Kind::kBool; }

  ABSL_ATTRIBUTE_PURE_FUNCTION static const BoolType& Get();

  BoolType() = default;

  BoolType(const BoolType&) = delete;
  BoolType(BoolType&&) = delete;
};

class IntType final : public Type {
 public:
  Kind kind() const override { return Kind::kInt; }

  absl::string_view name() const override { return "int"; }

 private:
  friend class IntValue;
  friend class TypeFactory;
  template <typename T>
  friend class internal::NoDestructor;
  friend class base_internal::TypeHandleBase;

  // Called by base_internal::TypeHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Type& type) { return type.kind() == Kind::kInt; }

  ABSL_ATTRIBUTE_PURE_FUNCTION static const IntType& Get();

  IntType() = default;

  IntType(const IntType&) = delete;
  IntType(IntType&&) = delete;
};

class UintType final : public Type {
 public:
  Kind kind() const override { return Kind::kUint; }

  absl::string_view name() const override { return "uint"; }

 private:
  friend class UintValue;
  friend class TypeFactory;
  template <typename T>
  friend class internal::NoDestructor;
  friend class base_internal::TypeHandleBase;

  // Called by base_internal::TypeHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Type& type) { return type.kind() == Kind::kUint; }

  ABSL_ATTRIBUTE_PURE_FUNCTION static const UintType& Get();

  UintType() = default;

  UintType(const UintType&) = delete;
  UintType(UintType&&) = delete;
};

class DoubleType final : public Type {
 public:
  Kind kind() const override { return Kind::kDouble; }

  absl::string_view name() const override { return "double"; }

 private:
  friend class DoubleValue;
  friend class TypeFactory;
  template <typename T>
  friend class internal::NoDestructor;
  friend class base_internal::TypeHandleBase;

  // Called by base_internal::TypeHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Type& type) { return type.kind() == Kind::kDouble; }

  ABSL_ATTRIBUTE_PURE_FUNCTION static const DoubleType& Get();

  DoubleType() = default;

  DoubleType(const DoubleType&) = delete;
  DoubleType(DoubleType&&) = delete;
};

class StringType final : public Type {
 public:
  Kind kind() const override { return Kind::kString; }

  absl::string_view name() const override { return "string"; }

 private:
  friend class StringValue;
  friend class TypeFactory;
  template <typename T>
  friend class internal::NoDestructor;
  friend class base_internal::TypeHandleBase;

  // Called by base_internal::TypeHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Type& type) { return type.kind() == Kind::kString; }

  ABSL_ATTRIBUTE_PURE_FUNCTION static const StringType& Get();

  StringType() = default;

  StringType(const StringType&) = delete;
  StringType(StringType&&) = delete;
};

class BytesType final : public Type {
 public:
  Kind kind() const override { return Kind::kBytes; }

  absl::string_view name() const override { return "bytes"; }

 private:
  friend class BytesValue;
  friend class TypeFactory;
  template <typename T>
  friend class internal::NoDestructor;
  friend class base_internal::TypeHandleBase;

  // Called by base_internal::TypeHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Type& type) { return type.kind() == Kind::kBytes; }

  ABSL_ATTRIBUTE_PURE_FUNCTION static const BytesType& Get();

  BytesType() = default;

  BytesType(const BytesType&) = delete;
  BytesType(BytesType&&) = delete;
};

class DurationType final : public Type {
 public:
  Kind kind() const override { return Kind::kDuration; }

  absl::string_view name() const override { return "google.protobuf.Duration"; }

 private:
  friend class DurationValue;
  friend class TypeFactory;
  template <typename T>
  friend class internal::NoDestructor;
  friend class base_internal::TypeHandleBase;

  // Called by base_internal::TypeHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Type& type) { return type.kind() == Kind::kDuration; }

  ABSL_ATTRIBUTE_PURE_FUNCTION static const DurationType& Get();

  DurationType() = default;

  DurationType(const DurationType&) = delete;
  DurationType(DurationType&&) = delete;
};

class TimestampType final : public Type {
 public:
  Kind kind() const override { return Kind::kTimestamp; }

  absl::string_view name() const override {
    return "google.protobuf.Timestamp";
  }

 private:
  friend class TimestampValue;
  friend class TypeFactory;
  template <typename T>
  friend class internal::NoDestructor;
  friend class base_internal::TypeHandleBase;

  // Called by base_internal::TypeHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Type& type) { return type.kind() == Kind::kTimestamp; }

  ABSL_ATTRIBUTE_PURE_FUNCTION static const TimestampType& Get();

  TimestampType() = default;

  TimestampType(const TimestampType&) = delete;
  TimestampType(TimestampType&&) = delete;
};

// EnumType represents an enumeration type. An enumeration is a set of constants
// that can be looked up by name and/or number.
class EnumType : public Type {
 public:
  struct Constant;

  class ConstantId final {
   public:
    explicit ConstantId(absl::string_view name)
        : data_(absl::in_place_type<absl::string_view>, name) {}

    explicit ConstantId(int64_t number)
        : data_(absl::in_place_type<int64_t>, number) {}

    ConstantId() = delete;

    ConstantId(const ConstantId&) = default;
    ConstantId& operator=(const ConstantId&) = default;

   private:
    friend class EnumType;
    friend class EnumValue;

    absl::variant<absl::string_view, int64_t> data_;
  };

  Kind kind() const final { return Kind::kEnum; }

  absl::Span<const Transient<const Type>> parameters() const final {
    return Type::parameters();
  }

  // Find the constant definition for the given identifier.
  absl::StatusOr<Constant> FindConstant(ConstantId id) const;

 protected:
  EnumType() = default;

  // Construct a new instance of EnumValue with a type of this. Called by
  // EnumValue::New.
  virtual absl::StatusOr<Persistent<const EnumValue>> NewInstanceByName(
      ValueFactory& value_factory, absl::string_view name) const = 0;

  // Construct a new instance of EnumValue with a type of this. Called by
  // EnumValue::New.
  virtual absl::StatusOr<Persistent<const EnumValue>> NewInstanceByNumber(
      ValueFactory& value_factory, int64_t number) const = 0;

  // Called by FindConstant.
  virtual absl::StatusOr<Constant> FindConstantByName(
      absl::string_view name) const = 0;

  // Called by FindConstant.
  virtual absl::StatusOr<Constant> FindConstantByNumber(
      int64_t number) const = 0;

 private:
  friend internal::TypeInfo base_internal::GetEnumTypeTypeId(
      const EnumType& enum_type);
  struct NewInstanceVisitor;
  struct FindConstantVisitor;

  friend struct NewInstanceVisitor;
  friend struct FindConstantVisitor;
  friend class EnumValue;
  friend class TypeFactory;
  friend class base_internal::TypeHandleBase;

  // Called by base_internal::TypeHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Type& type) { return type.kind() == Kind::kEnum; }

  EnumType(const EnumType&) = delete;
  EnumType(EnumType&&) = delete;

  std::pair<size_t, size_t> SizeAndAlignment() const override = 0;

  // Called by CEL_IMPLEMENT_ENUM_TYPE() and Is() to perform type checking.
  virtual internal::TypeInfo TypeId() const = 0;
};

// CEL_DECLARE_ENUM_TYPE declares `enum_type` as an enumeration type. It must be
// part of the class definition of `enum_type`.
//
// class MyEnumType : public cel::EnumType {
//  ...
// private:
//   CEL_DECLARE_ENUM_TYPE(MyEnumType);
// };
#define CEL_DECLARE_ENUM_TYPE(enum_type)                                       \
 private:                                                                      \
  friend class ::cel::base_internal::TypeHandleBase;                           \
                                                                               \
  static bool Is(const ::cel::Type& type);                                     \
                                                                               \
  ::std::pair<::std::size_t, ::std::size_t> SizeAndAlignment() const override; \
                                                                               \
  ::cel::internal::TypeInfo TypeId() const override;

// CEL_IMPLEMENT_ENUM_TYPE implements `enum_type` as an enumeration type. It
// must be called after the class definition of `enum_type`.
//
// class MyEnumType : public cel::EnumType {
//  ...
// private:
//   CEL_DECLARE_ENUM_TYPE(MyEnumType);
// };
//
// CEL_IMPLEMENT_ENUM_TYPE(MyEnumType);
#define CEL_IMPLEMENT_ENUM_TYPE(enum_type)                                  \
  static_assert(::std::is_base_of_v<::cel::EnumType, enum_type>,            \
                #enum_type " must inherit from cel::EnumType");             \
  static_assert(!::std::is_abstract_v<enum_type>,                           \
                "this must not be abstract");                               \
                                                                            \
  bool enum_type::Is(const ::cel::Type& type) {                             \
    return type.kind() == ::cel::Kind::kEnum &&                             \
           ::cel::base_internal::GetEnumTypeTypeId(                         \
               ::cel::internal::down_cast<const ::cel::EnumType&>(type)) == \
               ::cel::internal::TypeId<enum_type>();                        \
  }                                                                         \
                                                                            \
  ::std::pair<::std::size_t, ::std::size_t> enum_type::SizeAndAlignment()   \
      const {                                                               \
    static_assert(                                                          \
        ::std::is_same_v<enum_type,                                         \
                         ::std::remove_const_t<                             \
                             ::std::remove_reference_t<decltype(*this)>>>,  \
        "this must be the same as " #enum_type);                            \
    return ::std::pair<::std::size_t, ::std::size_t>(sizeof(enum_type),     \
                                                     alignof(enum_type));   \
  }                                                                         \
                                                                            \
  ::cel::internal::TypeInfo enum_type::TypeId() const {                     \
    return ::cel::internal::TypeId<enum_type>();                            \
  }

// StructType represents an struct type. An struct is a set of fields
// that can be looked up by name and/or number.
class StructType : public Type {
 public:
  struct Field;

  class FieldId final {
   public:
    explicit FieldId(absl::string_view name)
        : data_(absl::in_place_type<absl::string_view>, name) {}

    explicit FieldId(int64_t number)
        : data_(absl::in_place_type<int64_t>, number) {}

    FieldId() = delete;

    FieldId(const FieldId&) = default;
    FieldId& operator=(const FieldId&) = default;

   private:
    friend class StructType;

    absl::variant<absl::string_view, int64_t> data_;
  };

  Kind kind() const final { return Kind::kStruct; }

  absl::Span<const Transient<const Type>> parameters() const final {
    return Type::parameters();
  }

  // Find the field definition for the given identifier.
  absl::StatusOr<Field> FindField(TypeManager& type_manager, FieldId id) const;

 protected:
  StructType() = default;

  // TODO(issues/5): NewInstance

  // Called by FindField.
  virtual absl::StatusOr<Field> FindFieldByName(
      TypeManager& type_manager, absl::string_view name) const = 0;

  // Called by FindField.
  virtual absl::StatusOr<Field> FindFieldByNumber(TypeManager& type_manager,
                                                  int64_t number) const = 0;

 private:
  friend internal::TypeInfo base_internal::GetStructTypeTypeId(
      const StructType& struct_type);
  struct FindFieldVisitor;

  friend struct FindFieldVisitor;
  friend class TypeFactory;
  friend class base_internal::TypeHandleBase;

  // Called by base_internal::TypeHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Type& type) { return type.kind() == Kind::kStruct; }

  StructType(const StructType&) = delete;
  StructType(StructType&&) = delete;

  std::pair<size_t, size_t> SizeAndAlignment() const override = 0;

  // Called by CEL_IMPLEMENT_STRUCT_TYPE() and Is() to perform type checking.
  virtual internal::TypeInfo TypeId() const = 0;
};

// CEL_DECLARE_STRUCT_TYPE declares `struct_type` as an struct type. It must be
// part of the class definition of `struct_type`.
//
// class MyStructType : public cel::StructType {
//  ...
// private:
//   CEL_DECLARE_STRUCT_TYPE(MyStructType);
// };
#define CEL_DECLARE_STRUCT_TYPE(struct_type)                                   \
 private:                                                                      \
  friend class ::cel::base_internal::TypeHandleBase;                           \
                                                                               \
  static bool Is(const ::cel::Type& type);                                     \
                                                                               \
  ::std::pair<::std::size_t, ::std::size_t> SizeAndAlignment() const override; \
                                                                               \
  ::cel::internal::TypeInfo TypeId() const override;

// CEL_IMPLEMENT_ENUM_TYPE implements `struct_type` as an struct type. It
// must be called after the class definition of `struct_type`.
//
// class MyStructType : public cel::StructType {
//  ...
// private:
//   CEL_DECLARE_STRUCT_TYPE(MyStructType);
// };
//
// CEL_IMPLEMENT_STRUCT_TYPE(MyStructType);
#define CEL_IMPLEMENT_STRUCT_TYPE(struct_type)                                \
  static_assert(::std::is_base_of_v<::cel::StructType, struct_type>,          \
                #struct_type " must inherit from cel::StructType");           \
  static_assert(!::std::is_abstract_v<struct_type>,                           \
                "this must not be abstract");                                 \
                                                                              \
  bool struct_type::Is(const ::cel::Type& type) {                             \
    return type.kind() == ::cel::Kind::kStruct &&                             \
           ::cel::base_internal::GetStructTypeTypeId(                         \
               ::cel::internal::down_cast<const ::cel::StructType&>(type)) == \
               ::cel::internal::TypeId<struct_type>();                        \
  }                                                                           \
                                                                              \
  ::std::pair<::std::size_t, ::std::size_t> struct_type::SizeAndAlignment()   \
      const {                                                                 \
    static_assert(                                                            \
        ::std::is_same_v<struct_type,                                         \
                         ::std::remove_const_t<                               \
                             ::std::remove_reference_t<decltype(*this)>>>,    \
        "this must be the same as " #struct_type);                            \
    return ::std::pair<::std::size_t, ::std::size_t>(sizeof(struct_type),     \
                                                     alignof(struct_type));   \
  }                                                                           \
                                                                              \
  ::cel::internal::TypeInfo struct_type::TypeId() const {                     \
    return ::cel::internal::TypeId<struct_type>();                            \
  }

// ListType represents a list type. A list is a sequential container where each
// element is the same type.
class ListType : public Type {
  // I would have liked to make this class final, but we cannot instantiate
  // Persistent<const Type> or Transient<const Type> at this point. It must be
  // done after the post include below. Maybe we should separate out the post
  // includes on a per type basis so we can do that?
 public:
  Kind kind() const final { return Kind::kList; }

  absl::string_view name() const final { return "list"; }

  // Returns the type of the elements in the list.
  virtual Transient<const Type> element() const = 0;

 private:
  friend class TypeFactory;
  friend class base_internal::TypeHandleBase;
  friend class base_internal::ListTypeImpl;

  // Called by base_internal::TypeHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Type& type) { return type.kind() == Kind::kList; }

  ListType() = default;

  ListType(const ListType&) = delete;
  ListType(ListType&&) = delete;

  std::pair<size_t, size_t> SizeAndAlignment() const override = 0;

  // Called by base_internal::TypeHandleBase.
  bool Equals(const Type& other) const final;

  // Called by base_internal::TypeHandleBase.
  void HashValue(absl::HashState state) const final;
};

// MapType represents a map type. A map is container of key and value pairs
// where each key appears at most once.
class MapType : public Type {
  // I would have liked to make this class final, but we cannot instantiate
  // Persistent<const Type> or Transient<const Type> at this point. It must be
  // done after the post include below. Maybe we should separate out the post
  // includes on a per type basis so we can do that?
 public:
  Kind kind() const final { return Kind::kMap; }

  absl::string_view name() const final { return "map"; }

  // Returns the type of the keys in the map.
  virtual Transient<const Type> key() const = 0;

  // Returns the type of the values in the map.
  virtual Transient<const Type> value() const = 0;

 private:
  friend class TypeFactory;
  friend class base_internal::TypeHandleBase;
  friend class base_internal::MapTypeImpl;

  // Called by base_internal::TypeHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Type& type) { return type.kind() == Kind::kMap; }

  MapType() = default;

  MapType(const MapType&) = delete;
  MapType(MapType&&) = delete;

  std::pair<size_t, size_t> SizeAndAlignment() const override = 0;

  // Called by base_internal::TypeHandleBase.
  bool Equals(const Type& other) const final;

  // Called by base_internal::TypeHandleBase.
  void HashValue(absl::HashState state) const final;
};

}  // namespace cel

// type.pre.h forward declares types so they can be friended above. The types
// themselves need to be defined after everything else as they need to access or
// derive from the above types. We do this in type.post.h to avoid mudying this
// header and making it difficult to read.
#include "base/internal/type.post.h"  // IWYU pragma: export

namespace cel {

struct EnumType::Constant final {
  explicit Constant(absl::string_view name, int64_t number)
      : name(name), number(number) {}

  // The unqualified enumeration value name.
  absl::string_view name;
  // The enumeration value number.
  int64_t number;
};

struct StructType::Field final {
  explicit Field(absl::string_view name, int64_t number,
                 Persistent<const Type> type)
      : name(name), number(number), type(std::move(type)) {}

  // The field name.
  absl::string_view name;
  // The field number.
  int64_t number;
  // The field type;
  Persistent<const Type> type;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPE_H_
