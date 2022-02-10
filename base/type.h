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
#include "base/internal/type.h"
#include "base/kind.h"
#include "internal/reference_counted.h"

namespace cel {

class Value;

// A representation of a CEL type that enables reflection, for static analysis,
// and introspection, for program construction, of types.
class Type final {
 public:
  // Returns the null type.
  ABSL_ATTRIBUTE_PURE_FUNCTION static const Type& Null();

  // Returns the error type.
  ABSL_ATTRIBUTE_PURE_FUNCTION static const Type& Error();

  // Returns the dynamic type.
  ABSL_ATTRIBUTE_PURE_FUNCTION static const Type& Dyn();

  // Returns the any type.
  ABSL_ATTRIBUTE_PURE_FUNCTION static const Type& Any();

  // Returns the bool type.
  ABSL_ATTRIBUTE_PURE_FUNCTION static const Type& Bool();

  // Returns the int type.
  ABSL_ATTRIBUTE_PURE_FUNCTION static const Type& Int();

  // Returns the uint type.
  ABSL_ATTRIBUTE_PURE_FUNCTION static const Type& Uint();

  // Returns the double type.
  ABSL_ATTRIBUTE_PURE_FUNCTION static const Type& Double();

  // Returns the string type.
  ABSL_ATTRIBUTE_PURE_FUNCTION static const Type& String();

  // Returns the bytes type.
  ABSL_ATTRIBUTE_PURE_FUNCTION static const Type& Bytes();

  // Returns the duration type.
  ABSL_ATTRIBUTE_PURE_FUNCTION static const Type& Duration();

  // Returns the timestamp type.
  ABSL_ATTRIBUTE_PURE_FUNCTION static const Type& Timestamp();

  // Equivalent to `Type::Null()`.
  constexpr Type() : Type(nullptr) {}

  Type(const Type& other);

  Type(Type&& other);

  ~Type() { internal::Unref(impl_); }

  Type& operator=(const Type& other);

  Type& operator=(Type&& other);

  // Returns the type kind.
  Kind kind() const { return impl_ ? impl_->kind() : Kind::kNullType; }

  // Returns the type name, i.e. "list".
  absl::string_view name() const { return impl_ ? impl_->name() : "null_type"; }

  // Returns the type parameters of the type, i.e. key and value type of map.
  absl::Span<const Type> parameters() const {
    return impl_ ? impl_->parameters() : absl::Span<const Type>();
  }

  bool IsNull() const { return kind() == Kind::kNullType; }

  bool IsError() const { return kind() == Kind::kError; }

  bool IsDyn() const { return kind() == Kind::kDyn; }

  bool IsAny() const { return kind() == Kind::kAny; }

  bool IsBool() const { return kind() == Kind::kBool; }

  bool IsInt() const { return kind() == Kind::kInt; }

  bool IsUint() const { return kind() == Kind::kUint; }

  bool IsDouble() const { return kind() == Kind::kDouble; }

  bool IsString() const { return kind() == Kind::kString; }

  bool IsBytes() const { return kind() == Kind::kBytes; }

  bool IsDuration() const { return kind() == Kind::kDuration; }

  bool IsTimestamp() const { return kind() == Kind::kTimestamp; }

  template <typename H>
  friend H AbslHashValue(H state, const Type& type) {
    type.HashValue(absl::HashState::Create(&state));
    return std::move(state);
  }

  friend void swap(Type& lhs, Type& rhs) {
    const base_internal::BaseType* impl = lhs.impl_;
    lhs.impl_ = rhs.impl_;
    rhs.impl_ = impl;
  }

  friend bool operator==(const Type& lhs, const Type& rhs) {
    return lhs.Equals(rhs);
  }

  friend bool operator!=(const Type& lhs, const Type& rhs) {
    return !operator==(lhs, rhs);
  }

 private:
  friend class Value;

  static void Initialize();

  static const Type& Simple(Kind kind);

  constexpr explicit Type(const base_internal::BaseType* impl) : impl_(impl) {}

  bool Equals(const Type& other) const;

  void HashValue(absl::HashState state) const;

  const base_internal::BaseType* impl_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPE_H_
