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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPE_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPE_FACTORY_H_

#include "absl/strings/string_view.h"
#include "common/memory.h"
#include "common/sized_input_view.h"
#include "common/type.h"

namespace cel {

// `TypeFactory` is the preferred way for constructing compound types such as
// lists, maps, structs, and opaques. It caches types and avoids constructing
// them multiple times.
class TypeFactory {
 public:
  virtual ~TypeFactory() = default;

  // Returns a `MemoryManagerRef` which is used to manage memory for internal
  // data structures as well as created types.
  virtual MemoryManagerRef GetMemoryManager() const = 0;

  // Creates a `ListType` whose element type is `element`. Requires that
  // `element` is a valid element type for lists.
  ListType CreateListType(TypeView element);

  // Creates a `MapType` whose key type is `key` and value type is `value`.
  // Requires that `key` is a valid key type for maps and `value` is a valid
  // value type for maps.
  MapType CreateMapType(TypeView key, TypeView value);

  // Creates a `StructType` whose name is `name`. Requires that `name` is a
  // valid relative name, that is one or more `IDENT` (as defined by the Common
  // Expression language lexis) joined by `.`.
  StructType CreateStructType(absl::string_view name);

  // Creates a `OpaqueType` whose name is `name` and parameters are
  // `parameters`. Requires that `name` is a valid relative name, that is one or
  // more `IDENT` (as defined by the Common Expression language lexis) joined by
  // `.`, and that `parameters` contains zero or more valid parameter types for
  // opaques.
  OpaqueType CreateOpaqueType(absl::string_view name,
                              const SizedInputView<TypeView>& parameters);

  // Creates a `OptionalType`.
  OptionalType CreateOptionalType(TypeView parameter);

  // `GetDynListType` gets a view of the `ListType` type `list(dyn)`.
  ListTypeView GetDynListType();

  // `GetDynDynMapType` gets a view of the `MapType` type `map(dyn, dyn)`.
  MapTypeView GetDynDynMapType();

  // `GetDynDynMapType` gets a view of the `MapType` type `map(string, dyn)`.
  MapTypeView GetStringDynMapType();

  // `GetDynOptionalType` gets a view of the `OptionalType` type
  // `optional(dyn)`.
  OptionalTypeView GetDynOptionalType();

  NullType GetNullType() { return NullType{}; }

  ErrorType GetErrorType() { return ErrorType{}; }

  DynType GetDynType() { return DynType{}; }

  AnyType GetAnyType() { return AnyType{}; }

  BoolType GetBoolType() { return BoolType{}; }

  IntType GetIntType() { return IntType{}; }

  UintType GetUintType() { return UintType{}; }

  DoubleType GetDoubleType() { return DoubleType{}; }

  StringType GetStringType() { return StringType{}; }

  BytesType GetBytesType() { return BytesType{}; }

  DurationType GetDurationType() { return DurationType{}; }

  TimestampType GetTimestampType() { return TimestampType{}; }

  TypeType GetTypeType() { return TypeType{}; }

  UnknownType GetUnknownType() { return UnknownType{}; }

  BoolWrapperType GetBoolWrapperType() { return BoolWrapperType{}; }

  BytesWrapperType GetBytesWrapperType() { return BytesWrapperType{}; }

  DoubleWrapperType GetDoubleWrapperType() { return DoubleWrapperType{}; }

  IntWrapperType GetIntWrapperType() { return IntWrapperType{}; }

  StringWrapperType GetStringWrapperType() { return StringWrapperType{}; }

  UintWrapperType GetUintWrapperType() { return UintWrapperType{}; }

  Type GetJsonValueType() { return DynType{}; }

  ListType GetJsonListType() { return ListType(GetDynListType()); }

  MapType GetJsonMapType() { return MapType(GetStringDynMapType()); }

 private:
  virtual ListType CreateListTypeImpl(TypeView element) = 0;

  virtual MapType CreateMapTypeImpl(TypeView key, TypeView value) = 0;

  virtual StructType CreateStructTypeImpl(absl::string_view name) = 0;

  virtual OpaqueType CreateOpaqueTypeImpl(
      absl::string_view name, const SizedInputView<TypeView>& parameters) = 0;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPE_FACTORY_H_
