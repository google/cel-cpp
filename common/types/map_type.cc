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

#include <string>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "common/type.h"
#include "google/protobuf/arena.h"

namespace cel {

namespace common_internal {

namespace {

ABSL_CONST_INIT const MapTypeData kDynDynMapTypeData = {
    .key_and_value = {DynType(), DynType()},
};

ABSL_CONST_INIT const MapTypeData kStringDynMapTypeData = {
    .key_and_value = {StringType(), DynType()},
};

}  // namespace

absl::Nonnull<MapTypeData*> MapTypeData::Create(
    absl::Nonnull<google::protobuf::Arena*> arena, const Type& key, const Type& value) {
  MapTypeData* data =
      ::new (arena->AllocateAligned(sizeof(MapTypeData), alignof(MapTypeData)))
          MapTypeData;
  data->key_and_value[0] = key;
  data->key_and_value[1] = value;
  return data;
}

}  // namespace common_internal

MapType::MapType() : MapType(&common_internal::kDynDynMapTypeData) {}

MapType::MapType(absl::Nonnull<google::protobuf::Arena*> arena, const Type& key,
                 const Type& value)
    : MapType(key.IsDyn() && value.IsDyn()
                  ? &common_internal::kDynDynMapTypeData
                  : common_internal::MapTypeData::Create(arena, key, value)) {}

std::string MapType::DebugString() const {
  return absl::StrCat("map<", key().DebugString(), ", ", value().DebugString(),
                      ">");
}

absl::Span<const Type> MapType::parameters() const {
  return absl::MakeConstSpan(data_->key_and_value, 2);
}

const Type& MapType::key() const { return data_->key_and_value[0]; }

const Type& MapType::value() const { return data_->key_and_value[1]; }

MapType JsonMapType() {
  return MapType(&common_internal::kStringDynMapTypeData);
}

}  // namespace cel
