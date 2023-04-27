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

#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/optimization.h"
#include "absl/hash/hash.h"
#include "absl/strings/string_view.h"
#include "absl/utility/utility.h"
#include "base/handle.h"
#include "base/internal/data.h"
#include "base/internal/type.h"  // IWYU pragma: export
#include "base/kind.h"
#include "internal/casts.h"  // IWYU pragma: keep

namespace cel {

class Value;
class EnumType;
class StructType;
class ListType;
class MapType;
class TypeFactory;
class TypeProvider;
class TypeManager;
class WrapperType;
class OpaqueType;
class ValueFactory;

// A representation of a CEL type that enables introspection, for program
// construction, of types.
class Type : public base_internal::Data {
 public:
  static bool Is(const Type& type ABSL_ATTRIBUTE_UNUSED) { return true; }

  static const Type& Cast(const Type& type) { return type; }

  // Returns the type kind.
  Kind kind() const { return base_internal::Metadata::Kind(*this); }

  // Returns the type name, i.e. "list".
  absl::string_view name() const;

  std::string DebugString() const;

  void HashValue(absl::HashState state) const;

  bool Equals(const Type& other) const;

  template <typename T>
  bool Is() const {
    static_assert(!std::is_const_v<T>, "T must not be const");
    static_assert(!std::is_volatile_v<T>, "T must not be volatile");
    static_assert(!std::is_pointer_v<T>, "T must not be a pointer");
    static_assert(!std::is_reference_v<T>, "T must not be a reference");
    static_assert(std::is_base_of_v<Type, T>, "T must be derived from Type");
    return T::Is(*this);
  }

  template <typename T>
  const T& As() const {
    static_assert(!std::is_const_v<T>, "T must not be const");
    static_assert(!std::is_volatile_v<T>, "T must not be volatile");
    static_assert(!std::is_pointer_v<T>, "T must not be a pointer");
    static_assert(!std::is_reference_v<T>, "T must not be a reference");
    static_assert(std::is_base_of_v<Type, T>, "T must be derived from Type");
    return T::Cast(*this);
  }

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const Type& type) {
    sink.Append(type.DebugString());
  }

 private:
  friend class EnumType;
  friend class StructType;
  friend class ListType;
  friend class MapType;
  template <Kind K>
  friend class base_internal::SimpleType;
  friend class WrapperType;
  friend class base_internal::TypeHandle;
  friend class OpaqueType;

  static bool Equals(const Type& lhs, const Type& rhs, Kind kind);

  static bool Equals(const Type& lhs, const Type& rhs) {
    if (&lhs == &rhs) {
      return true;
    }
    Kind lhs_kind = lhs.kind();
    return lhs_kind == rhs.kind() && Equals(lhs, rhs, lhs_kind);
  }

  static void HashValue(const Type& type, Kind kind, absl::HashState state);

  static void HashValue(const Type& type, absl::HashState state) {
    HashValue(type, type.kind(), std::move(state));
  }

  Type() = default;
  Type(const Type&) = default;
  Type(Type&&) = default;
  Type& operator=(const Type&) = default;
  Type& operator=(Type&&) = default;
};

}  // namespace cel

// -----------------------------------------------------------------------------
// Internal implementation details.

namespace cel {

namespace base_internal {

class TypeHandle final {
 public:
  TypeHandle() = default;

  template <typename T, typename... Args>
  explicit TypeHandle(absl::in_place_type_t<T>, Args&&... args) {
    data_.ConstructInline<T>(std::forward<Args>(args)...);
  }

  explicit TypeHandle(const Type& type) { data_.ConstructHeap(type); }

  TypeHandle(const TypeHandle& other) { CopyFrom(other); }

  TypeHandle(TypeHandle&& other) { MoveFrom(other); }

  ~TypeHandle() { Destruct(); }

  TypeHandle& operator=(const TypeHandle& other) {
    if (ABSL_PREDICT_TRUE(this != &other)) {
      CopyAssign(other);
    }
    return *this;
  }

  TypeHandle& operator=(TypeHandle&& other) {
    if (ABSL_PREDICT_TRUE(this != &other)) {
      MoveAssign(other);
    }
    return *this;
  }

  Type* get() const { return static_cast<Type*>(data_.get()); }

  explicit operator bool() const { return !data_.IsNull(); }

  bool Equals(const TypeHandle& other) const;

  void HashValue(absl::HashState state) const;

  Type* release() {
    Type* type = static_cast<Type*>(data_.get_heap());
    data_.set_pointer(0);
    return type;
  }

 private:
  static bool Equals(const Type& lhs, const Type& rhs, Kind kind);

  static void HashValue(const Type& type, Kind kind, absl::HashState state);

  void CopyFrom(const TypeHandle& other);

  void MoveFrom(TypeHandle& other);

  void CopyAssign(const TypeHandle& other);

  void MoveAssign(TypeHandle& other);

  void Ref() const { data_.Ref(); }

  void Unref() const {
    if (data_.Unref()) {
      Delete();
    }
  }

  void Destruct();

  void Delete() const;

  AnyType data_;
};

template <typename H>
H AbslHashValue(H state, const TypeHandle& handle) {
  handle.HashValue(absl::HashState::Create(&state));
  return state;
}

inline bool operator==(const TypeHandle& lhs, const TypeHandle& rhs) {
  return lhs.Equals(rhs);
}

inline bool operator!=(const TypeHandle& lhs, const TypeHandle& rhs) {
  return !operator==(lhs, rhs);
}

// Specialization for Type providing the implementation to `Handle`.
template <>
struct HandleTraits<Type> {
  using handle_type = TypeHandle;
};

// Partial specialization for `Handle` for all classes derived from Type.
template <typename T>
struct HandleTraits<T, std::enable_if_t<(std::is_base_of_v<Type, T> &&
                                         !std::is_same_v<Type, T>)>>
    final : public HandleTraits<Type> {};

template <Kind K>
struct SimpleTypeName;

template <>
struct SimpleTypeName<Kind::kNullType> {
  static constexpr absl::string_view value = "null_type";
};

template <>
struct SimpleTypeName<Kind::kError> {
  static constexpr absl::string_view value = "*error*";
};

template <>
struct SimpleTypeName<Kind::kDyn> {
  static constexpr absl::string_view value = "dyn";
};

template <>
struct SimpleTypeName<Kind::kAny> {
  static constexpr absl::string_view value = "google.protobuf.Any";
};

template <>
struct SimpleTypeName<Kind::kBool> {
  static constexpr absl::string_view value = "bool";
};

template <>
struct SimpleTypeName<Kind::kInt> {
  static constexpr absl::string_view value = "int";
};

template <>
struct SimpleTypeName<Kind::kUint> {
  static constexpr absl::string_view value = "uint";
};

template <>
struct SimpleTypeName<Kind::kDouble> {
  static constexpr absl::string_view value = "double";
};

template <>
struct SimpleTypeName<Kind::kBytes> {
  static constexpr absl::string_view value = "bytes";
};

template <>
struct SimpleTypeName<Kind::kString> {
  static constexpr absl::string_view value = "string";
};

template <>
struct SimpleTypeName<Kind::kDuration> {
  static constexpr absl::string_view value = "google.protobuf.Duration";
};

template <>
struct SimpleTypeName<Kind::kTimestamp> {
  static constexpr absl::string_view value = "google.protobuf.Timestamp";
};

template <>
struct SimpleTypeName<Kind::kType> {
  static constexpr absl::string_view value = "type";
};

template <>
struct SimpleTypeName<Kind::kUnknown> {
  static constexpr absl::string_view value = "*unknown*";
};

template <Kind K>
class SimpleType : public Type, public InlineData {
 public:
  static constexpr Kind kKind = K;
  static constexpr absl::string_view kName = SimpleTypeName<K>::value;

  static bool Is(const Type& type) { return type.kind() == kKind; }

  using Type::Is;

  constexpr SimpleType() : InlineData(kMetadata) {}

  SimpleType(const SimpleType&) = default;
  SimpleType(SimpleType&&) = default;
  SimpleType& operator=(const SimpleType&) = default;
  SimpleType& operator=(SimpleType&&) = default;

  constexpr Kind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  std::string DebugString() const { return std::string(name()); }

 private:
  friend class TypeHandle;

  static constexpr uintptr_t kMetadata =
      kStoredInline | kTrivial | (static_cast<uintptr_t>(kKind) << kKindShift);
};

template <>
struct TypeTraits<Type> {
  using type = Type;

  using value_type = Value;
};

}  // namespace base_internal

CEL_INTERNAL_TYPE_DECL(Type);

}  // namespace cel

#define CEL_INTERNAL_SIMPLE_TYPE_MEMBERS(type_class, value_class)      \
 private:                                                              \
  friend class value_class;                                            \
  friend class TypeFactory;                                            \
  friend class base_internal::TypeHandle;                              \
  template <typename T, typename U>                                    \
  friend class base_internal::SimpleValue;                             \
  template <size_t Size, size_t Align>                                 \
  friend class base_internal::AnyData;                                 \
                                                                       \
  ABSL_ATTRIBUTE_PURE_FUNCTION static const Handle<type_class>& Get(); \
                                                                       \
  type_class() = default;                                              \
  type_class(const type_class&) = default;                             \
  type_class(type_class&&) = default;                                  \
  type_class& operator=(const type_class&) = default;                  \
  type_class& operator=(type_class&&) = default

#define CEL_INTERNAL_SIMPLE_TYPE_STANDALONES(type_class)        \
  static_assert(std::is_trivially_copyable_v<type_class>,       \
                #type_class " must be trivially copyable");     \
  static_assert(std::is_trivially_destructible_v<type_class>,   \
                #type_class " must be trivially destructible"); \
                                                                \
  CEL_INTERNAL_TYPE_DECL(type_class)

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPE_H_
