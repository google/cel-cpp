// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_BASE_INTERNAL_TYPE_H_
#define THIRD_PARTY_CEL_CPP_BASE_INTERNAL_TYPE_H_

#include "absl/hash/hash.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/kind.h"
#include "internal/reference_counted.h"

namespace cel {

class Type;

namespace base_internal {

class SimpleType;

class BaseType : public cel::internal::ReferenceCounted {
 public:
  // Returns the type kind.
  virtual Kind kind() const = 0;

  // Returns the type name, i.e. map or google.protobuf.Any.
  virtual absl::string_view name() const = 0;

  // Returns the type parameters of the type, i.e. key and value of map type.
  virtual absl::Span<const cel::Type> parameters() const = 0;

 protected:
  // Overriden by subclasses to implement more strictly equality testing. By
  // default `cel::Type` ensures `kind()` and `name()` are equal, this behavior
  // cannot be overriden. It is completely valid and acceptable to simply return
  // `true`.
  //
  // This method should only ever be called by cel::Type.
  virtual bool Equals(const cel::Type& value) const = 0;

  // Overriden by subclasses to implement better hashing. By default `cel::Type`
  // hashes `kind()` and `name()`, this behavior cannot be overriden. It is
  // completely valid and acceptable to simply do nothing.
  //
  // This method should only ever be called by cel::Type.
  virtual void HashValue(absl::HashState state) const = 0;

 private:
  friend class cel::Type;
  friend class SimpleType;

  // The default constructor is private so that only sanctioned classes can
  // extend it. Users should extend those classes instead of this one.
  constexpr BaseType() = default;
};

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_INTERNAL_TYPE_H_
