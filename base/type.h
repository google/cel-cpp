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

#include "absl/hash/hash.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
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
class EnumType;
class ListType;
class MapType;
class TypeType;
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
class StructValue;
class TypeValue;
class ValueFactory;
class TypedEnumValueFactory;
class TypedStructValueFactory;

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

  virtual std::string DebugString() const;

  // Called by base_internal::TypeHandleBase.
  // Note GCC does not consider a friend member as a member of a friend.
  virtual bool Equals(const Type& other) const;

  // Called by base_internal::TypeHandleBase.
  // Note GCC does not consider a friend member as a member of a friend.
  virtual void HashValue(absl::HashState state) const;

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
  friend class TypeType;
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
};

}  // namespace cel

// type.pre.h forward declares types so they can be friended above. The types
// themselves need to be defined after everything else as they need to access or
// derive from the above types. We do this in type.post.h to avoid mudying this
// header and making it difficult to read.
#include "base/internal/type.post.h"  // IWYU pragma: export

namespace cel {

CEL_INTERNAL_TYPE_DECL(Type);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPE_H_
