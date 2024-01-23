// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "base/type_provider.h"

#include <algorithm>
#include <array>
#include <utility>

#include "absl/base/no_destructor.h"
#include "base/type_factory.h"

namespace cel {

namespace {

using base_internal::HandleFactory;

class BuiltinTypeProvider final : public TypeProvider {
 public:
  using BuiltinType = std::pair<absl::string_view,
                                absl::StatusOr<Handle<Type>> (*)(TypeFactory&)>;

  BuiltinTypeProvider()
      : types_{{
            {"null_type", GetNullType},
            {"google.protobuf.NullValue", GetNullType},
            {"bool", GetBoolType},
            {"int", GetIntType},
            {"uint", GetUintType},
            {"double", GetDoubleType},
            {"bytes", GetBytesType},
            {"string", GetStringType},
            {"google.protobuf.Duration", GetDurationType},
            {"google.protobuf.Timestamp", GetTimestampType},
            {"list", GetListType},
            {"google.protobuf.ListValue", GetListType},
            {"map", GetMapType},
            {"google.protobuf.Struct", GetStructType},
            {"type", GetTypeType},
            {"google.protobuf.Value", GetValueType},
            {"google.protobuf.Any", GetAnyType},
            {"google.protobuf.BoolValue", GetBoolWrapperType},
            {"google.protobuf.BytesValue", GetBytesWrapperType},
            {"google.protobuf.DoubleValue", GetDoubleWrapperType},
            {"google.protobuf.FloatValue", GetDoubleWrapperType},
            {"google.protobuf.Int32Value", GetIntWrapperType},
            {"google.protobuf.Int64Value", GetIntWrapperType},
            {"google.protobuf.StringValue", GetStringWrapperType},
            {"google.protobuf.UInt32Value", GetUintWrapperType},
            {"google.protobuf.UInt64Value", GetUintWrapperType},
        }} {
    std::stable_sort(
        types_.begin(), types_.end(),
        [](const BuiltinType& lhs, const BuiltinType& rhs) -> bool {
          return lhs.first < rhs.first;
        });
  }

  absl::StatusOr<absl::optional<Handle<Type>>> ProvideType(
      TypeFactory& type_factory, absl::string_view name) const override {
    auto existing = std::lower_bound(
        types_.begin(), types_.end(), name,
        [](const BuiltinType& lhs, absl::string_view rhs) -> bool {
          return lhs.first < rhs;
        });
    if (existing == types_.end() || existing->first != name) {
      return absl::nullopt;
    }
    return (existing->second)(type_factory);
  }

 private:
  static absl::StatusOr<Handle<Type>> GetNullType(TypeFactory& type_factory) {
    return type_factory.GetNullType();
  }

  static absl::StatusOr<Handle<Type>> GetBoolType(TypeFactory& type_factory) {
    return type_factory.GetBoolType();
  }

  static absl::StatusOr<Handle<Type>> GetIntType(TypeFactory& type_factory) {
    return type_factory.GetIntType();
  }

  static absl::StatusOr<Handle<Type>> GetUintType(TypeFactory& type_factory) {
    return type_factory.GetUintType();
  }

  static absl::StatusOr<Handle<Type>> GetDoubleType(TypeFactory& type_factory) {
    return type_factory.GetDoubleType();
  }

  static absl::StatusOr<Handle<Type>> GetBytesType(TypeFactory& type_factory) {
    return type_factory.GetBytesType();
  }

  static absl::StatusOr<Handle<Type>> GetStringType(TypeFactory& type_factory) {
    return type_factory.GetStringType();
  }

  static absl::StatusOr<Handle<Type>> GetDurationType(
      TypeFactory& type_factory) {
    return type_factory.GetDurationType();
  }

  static absl::StatusOr<Handle<Type>> GetTimestampType(
      TypeFactory& type_factory) {
    return type_factory.GetTimestampType();
  }

  static absl::StatusOr<Handle<Type>> GetBoolWrapperType(
      TypeFactory& type_factory) {
    return type_factory.GetBoolWrapperType();
  }

  static absl::StatusOr<Handle<Type>> GetBytesWrapperType(
      TypeFactory& type_factory) {
    return type_factory.GetBytesWrapperType();
  }

  static absl::StatusOr<Handle<Type>> GetDoubleWrapperType(
      TypeFactory& type_factory) {
    return type_factory.GetDoubleWrapperType();
  }

  static absl::StatusOr<Handle<Type>> GetIntWrapperType(
      TypeFactory& type_factory) {
    return type_factory.GetIntWrapperType();
  }

  static absl::StatusOr<Handle<Type>> GetStringWrapperType(
      TypeFactory& type_factory) {
    return type_factory.GetStringWrapperType();
  }

  static absl::StatusOr<Handle<Type>> GetUintWrapperType(
      TypeFactory& type_factory) {
    return type_factory.GetUintWrapperType();
  }

  static absl::StatusOr<Handle<Type>> GetListType(TypeFactory& type_factory) {
    // The element type does not matter.
    return HandleFactory<ListType>::Make<base_internal::LegacyListType>();
  }

  static absl::StatusOr<Handle<Type>> GetMapType(TypeFactory& type_factory) {
    // The key and value types do not matter.
    return HandleFactory<MapType>::Make<base_internal::LegacyMapType>();
  }

  static absl::StatusOr<Handle<Type>> GetStructType(TypeFactory& type_factory) {
    return type_factory.CreateMapType(type_factory.GetStringType(),
                                      type_factory.GetDynType());
  }

  static absl::StatusOr<Handle<Type>> GetTypeType(TypeFactory& type_factory) {
    return type_factory.GetTypeType();
  }

  static absl::StatusOr<Handle<Type>> GetValueType(TypeFactory& type_factory) {
    return type_factory.GetDynType();
  }

  static absl::StatusOr<Handle<Type>> GetAnyType(TypeFactory& type_factory) {
    return type_factory.GetAnyType();
  }

  std::array<BuiltinType, 26> types_;
};

}  // namespace

TypeProvider& TypeProvider::Builtin() {
  static absl::NoDestructor<BuiltinTypeProvider> instance;
  return *instance;
}

}  // namespace cel
