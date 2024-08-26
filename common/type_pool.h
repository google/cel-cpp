// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPE_POOL_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPE_POOL_H_

#include <memory>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/arena_string.h"
#include "common/arena_string_pool.h"
#include "common/type.h"
#include "common/types/function_type_pool.h"
#include "common/types/list_type_pool.h"
#include "common/types/map_type_pool.h"
#include "common/types/opaque_type_pool.h"
#include "common/types/type_type_pool.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"

namespace cel {

class TypePool;

absl::Nonnull<std::unique_ptr<TypePool>> NewTypePool(
    absl::Nonnull<google::protobuf::Arena*> arena ABSL_ATTRIBUTE_LIFETIME_BOUND,
    absl::Nonnull<ArenaStringPool*> string_pool ABSL_ATTRIBUTE_LIFETIME_BOUND,
    absl::Nullable<const google::protobuf::DescriptorPool*> descriptor_pool
        ABSL_ATTRIBUTE_LIFETIME_BOUND);

class TypePool final {
 public:
  TypePool(const TypePool&) = delete;
  TypePool(TypePool&&) = delete;
  TypePool& operator=(const TypePool&) = delete;
  TypePool& operator=(TypePool&&) = delete;

  ListType MakeListType(const Type& element);

  MapType MakeMapType(const Type& key, const Type& value);

  StructType MakeStructType(absl::string_view name);

  StructType MakeStructType(ArenaString) = delete;

  FunctionType MakeFunctionType(const Type& result,
                                absl::Span<const Type> args);

  OpaqueType MakeOpaqueType(absl::string_view name,
                            absl::Span<const Type> params);

  OpaqueType MakeOpaqueType(ArenaString, absl::Span<const Type>) = delete;

  OptionalType MakeOptionalType(const Type& param) {
    return static_cast<OptionalType>(
        MakeOpaqueType(OptionalType::kName, absl::MakeConstSpan(&param, 1)));
  }

  TypeParamType MakeTypeParamType(absl::string_view name);

  TypeParamType MakeTypeParamType(ArenaString) = delete;

  TypeType MakeTypeType(const Type& type);

 private:
  friend absl::Nonnull<std::unique_ptr<TypePool>> NewTypePool(
      absl::Nonnull<google::protobuf::Arena*>, absl::Nonnull<ArenaStringPool*>,
      absl::Nullable<const google::protobuf::DescriptorPool*>);

  TypePool(absl::Nonnull<google::protobuf::Arena*> arena,
           absl::Nonnull<ArenaStringPool*> string_pool,
           absl::Nullable<const google::protobuf::DescriptorPool*> descriptor_pool)
      : string_pool_(string_pool),
        descriptor_pool_(descriptor_pool),
        function_type_pool_(arena),
        list_type_pool_(arena),
        map_type_pool_(arena),
        opaque_type_pool_(arena),
        type_type_pool_(arena) {}

  absl::Nonnull<ArenaStringPool*> const string_pool_;
  absl::Nullable<const google::protobuf::DescriptorPool*> const descriptor_pool_;
  common_internal::FunctionTypePool function_type_pool_;
  common_internal::ListTypePool list_type_pool_;
  common_internal::MapTypePool map_type_pool_;
  common_internal::OpaqueTypePool opaque_type_pool_;
  common_internal::TypeTypePool type_type_pool_;
};

inline absl::Nonnull<std::unique_ptr<TypePool>> NewTypePool(
    absl::Nonnull<google::protobuf::Arena*> arena ABSL_ATTRIBUTE_LIFETIME_BOUND,
    absl::Nonnull<ArenaStringPool*> string_pool ABSL_ATTRIBUTE_LIFETIME_BOUND,
    absl::Nullable<const google::protobuf::DescriptorPool*> descriptor_pool
        ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return std::unique_ptr<TypePool>(
      new TypePool(arena, string_pool, descriptor_pool));
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPE_POOL_H_
