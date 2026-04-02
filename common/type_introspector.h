// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPE_INTROSPECTOR_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPE_INTROSPECTOR_H_

#include <cstdint>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/type.h"

namespace cel {

// `TypeIntrospector` is an interface which allows querying type-related
// information. It handles type introspection, but not type reflection. That is,
// it is not capable of instantiating new values or understanding values. Its
// primary usage is for type checking, and a subset of that shared functionality
// is used by the runtime.
class TypeIntrospector {
 public:
  struct EnumConstant {
    // The type of the enum. For JSON null, this may be a specific type rather
    // than an enum type.
    Type type;
    absl::string_view type_full_name;
    absl::string_view value_name;
    int32_t number;
  };

  struct StructTypeFieldListing {
    // The name used to access the field in source CEL.
    // This is assumed owned by the TypeIntrospector or a dependency that
    // outlives it.
    absl::string_view name;
    // The field description.
    StructTypeField field;
  };

  virtual ~TypeIntrospector() = default;

  // `FindType` find the type corresponding to name `name`.
  absl::StatusOr<absl::optional<Type>> FindType(absl::string_view name) const {
    return FindTypeImpl(name);
  }

  // `FindEnumConstant` find a fully qualified enumerator name `name` in enum
  // type `type`.
  absl::StatusOr<absl::optional<EnumConstant>> FindEnumConstant(
      absl::string_view type, absl::string_view value) const {
    return FindEnumConstantImpl(type, value);
  }

  // `FindStructTypeFieldByName` find the name, number, and type of the field
  // `name` in type `type`.
  absl::StatusOr<absl::optional<StructTypeField>> FindStructTypeFieldByName(
      absl::string_view type, absl::string_view name) const {
    return FindStructTypeFieldByNameImpl(type, name);
  }

  // `ListFieldsForStructType` returns the fields of struct type `type`.
  //
  // This is used when the struct is declared as a context type.
  //
  // If the type is not found, returns `absl::nullopt`.
  // If the type exists but is not a struct or has no fields, returns an empty
  // vector.
  absl::StatusOr<absl::optional<std::vector<StructTypeFieldListing>>>
  ListFieldsForStructType(absl::string_view type) const {
    return ListFieldsForStructTypeImpl(type);
  }

  // `FindStructTypeFieldByName` find the name, number, and type of the field
  // `name` in struct type `type`.
  absl::StatusOr<absl::optional<StructTypeField>> FindStructTypeFieldByName(
      const StructType& type, absl::string_view name) const {
    return FindStructTypeFieldByName(type.name(), name);
  }

 protected:
  virtual absl::StatusOr<absl::optional<Type>> FindTypeImpl(
      absl::string_view name) const;

  virtual absl::StatusOr<absl::optional<EnumConstant>> FindEnumConstantImpl(
      absl::string_view type, absl::string_view value) const;

  virtual absl::StatusOr<absl::optional<StructTypeField>>
  FindStructTypeFieldByNameImpl(absl::string_view type,
                                absl::string_view name) const;

  virtual absl::StatusOr<absl::optional<std::vector<StructTypeFieldListing>>>
  ListFieldsForStructTypeImpl(absl::string_view type) const;
};

// Looks up a well-known type by name.
absl::optional<Type> FindWellKnownType(absl::string_view name);

// Looks up a well-known enum constant by type and value.
absl::optional<TypeIntrospector::EnumConstant> FindWellKnownTypeEnumConstant(
    absl::string_view type, absl::string_view value);

// Looks up a well-known struct type field by type and field name.
absl::optional<StructTypeField> FindWellKnownTypeFieldByName(
    absl::string_view type, absl::string_view name);

absl::optional<std::vector<TypeIntrospector::StructTypeFieldListing>>
ListFieldsForWellKnownType(absl::string_view type);

// `WellKnownTypeIntrospector` is an implementation of `TypeIntrospector` which
// handles well known types that are treated specially by CEL.
//
// This also serves as a minimal implementation of a TypeInstrospector when no
// custom types are present.
//
// This class has no mutable state, so trivially thread-safe.
class WellKnownTypeIntrospector : public virtual TypeIntrospector {
 public:
  WellKnownTypeIntrospector() = default;

 private:
  absl::StatusOr<absl::optional<Type>> FindTypeImpl(
      absl::string_view name) const final {
    return FindWellKnownType(name);
  }

  absl::StatusOr<absl::optional<EnumConstant>> FindEnumConstantImpl(
      absl::string_view type, absl::string_view value) const final {
    return FindWellKnownTypeEnumConstant(type, value);
  }

  absl::StatusOr<absl::optional<StructTypeField>> FindStructTypeFieldByNameImpl(
      absl::string_view type, absl::string_view name) const final {
    return FindWellKnownTypeFieldByName(type, name);
  }

  absl::StatusOr<absl::optional<std::vector<StructTypeFieldListing>>>
  ListFieldsForStructTypeImpl(absl::string_view type) const final {
    return ListFieldsForWellKnownType(type);
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPE_INTROSPECTOR_H_
