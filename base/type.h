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

#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/hash/hash.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/handle.h"
#include "base/internal/type.pre.h"  // IWYU pragma: export
#include "base/kind.h"
#include "base/memory_manager.h"

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
class TypeFactory;

class NullValue;
class ErrorValue;
class BoolValue;
class IntValue;
class UintValue;
class DoubleValue;
class BytesValue;
class DurationValue;
class TimestampValue;
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

}  // namespace cel

// type.pre.h forward declares types so they can be friended above. The types
// themselves need to be defined after everything else as they need to access or
// derive from the above types. We do this in type.post.h to avoid mudying this
// header and making it difficult to read.
#include "base/internal/type.post.h"  // IWYU pragma: export

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPE_H_
