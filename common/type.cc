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

#include "common/type.h"

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "common/types/type_cache.h"
#include "common/types/types.h"
#include "google/protobuf/descriptor.h"

namespace cel {

using ::google::protobuf::Descriptor;

Type Type::Message(absl::Nonnull<const Descriptor*> descriptor) {
  switch (descriptor->well_known_type()) {
    case Descriptor::WELLKNOWNTYPE_BOOLVALUE:
      return BoolWrapperType();
    case Descriptor::WELLKNOWNTYPE_INT32VALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case Descriptor::WELLKNOWNTYPE_INT64VALUE:
      return IntWrapperType();
    case Descriptor::WELLKNOWNTYPE_UINT32VALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case Descriptor::WELLKNOWNTYPE_UINT64VALUE:
      return UintWrapperType();
    case Descriptor::WELLKNOWNTYPE_FLOATVALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case Descriptor::WELLKNOWNTYPE_DOUBLEVALUE:
      return DoubleWrapperType();
    case Descriptor::WELLKNOWNTYPE_BYTESVALUE:
      return BytesWrapperType();
    case Descriptor::WELLKNOWNTYPE_STRINGVALUE:
      return StringWrapperType();
    case Descriptor::WELLKNOWNTYPE_ANY:
      return AnyType();
    case Descriptor::WELLKNOWNTYPE_DURATION:
      return DurationType();
    case Descriptor::WELLKNOWNTYPE_TIMESTAMP:
      return TimestampType();
    case Descriptor::WELLKNOWNTYPE_VALUE:
      return DynType();
    case Descriptor::WELLKNOWNTYPE_LISTVALUE:
      return ListType();
    case Descriptor::WELLKNOWNTYPE_STRUCT:
      return common_internal::ProcessLocalTypeCache::Get()
          ->GetStringDynMapType();
    default:
      return MessageType(descriptor);
  }
}

common_internal::StructTypeVariant Type::ToStructTypeVariant() const {
  if (const auto* other = absl::get_if<MessageType>(&variant_);
      other != nullptr) {
    return common_internal::StructTypeVariant(*other);
  }
  if (const auto* other =
          absl::get_if<common_internal::BasicStructType>(&variant_);
      other != nullptr) {
    return common_internal::StructTypeVariant(*other);
  }
  return common_internal::StructTypeVariant();
}

absl::optional<StructType> Type::AsStruct() const {
  if (const auto* alt =
          absl::get_if<common_internal::BasicStructType>(&variant_);
      alt != nullptr) {
    return *alt;
  }
  if (const auto* alt = absl::get_if<MessageType>(&variant_); alt != nullptr) {
    return *alt;
  }
  return absl::nullopt;
}

absl::optional<MessageType> Type::AsMessage() const {
  if (const auto* alt = absl::get_if<MessageType>(&variant_); alt != nullptr) {
    return *alt;
  }
  return absl::nullopt;
}

Type::operator StructType() const {
  ABSL_DCHECK(IsStruct()) << DebugString();
  if (const auto* alt =
          absl::get_if<common_internal::BasicStructType>(&variant_);
      alt != nullptr) {
    return *alt;
  }
  if (const auto* alt = absl::get_if<MessageType>(&variant_); alt != nullptr) {
    return *alt;
  }
  return StructType();
}

Type::operator MessageType() const {
  ABSL_DCHECK(IsMessage()) << DebugString();
  if (const auto* alt = absl::get_if<MessageType>(&variant_);
      ABSL_PREDICT_TRUE(alt != nullptr)) {
    return *alt;
  }
  return MessageType();
}

}  // namespace cel
