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

#include <array>
#include <string>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "common/type_kind.h"
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
      return JsonMapType();
    default:
      return MessageType(descriptor);
  }
}

Type Type::Enum(absl::Nonnull<const google::protobuf::EnumDescriptor*> descriptor) {
  if (descriptor->full_name() == "google.protobuf.NullValue") {
    return NullType();
  }
  return EnumType(descriptor);
}

namespace {

static constexpr std::array<TypeKind, 28> kTypeToKindArray = {
    TypeKind::kDyn,         TypeKind::kAny,           TypeKind::kBool,
    TypeKind::kBoolWrapper, TypeKind::kBytes,         TypeKind::kBytesWrapper,
    TypeKind::kDouble,      TypeKind::kDoubleWrapper, TypeKind::kDuration,
    TypeKind::kEnum,        TypeKind::kError,         TypeKind::kFunction,
    TypeKind::kInt,         TypeKind::kIntWrapper,    TypeKind::kList,
    TypeKind::kMap,         TypeKind::kNull,          TypeKind::kOpaque,
    TypeKind::kString,      TypeKind::kStringWrapper, TypeKind::kStruct,
    TypeKind::kStruct,      TypeKind::kTimestamp,     TypeKind::kTypeParam,
    TypeKind::kType,        TypeKind::kUint,          TypeKind::kUintWrapper,
    TypeKind::kUnknown};

static_assert(kTypeToKindArray.size() ==
                  absl::variant_size<common_internal::TypeVariant>(),
              "Kind indexer must match variant declaration for cel::Type.");

}  // namespace

TypeKind Type::kind() const {
  return kTypeToKindArray[variant_.index()];
}

absl::string_view Type::name() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return absl::visit(
      [](const auto& alternative) -> absl::string_view {
        return alternative.name();
      },
      variant_);
}

std::string Type::DebugString() const {
  return absl::visit(
      [](const auto& alternative) -> std::string {
        return alternative.DebugString();
      },
      variant_);
}

absl::Span<const Type> Type::parameters() const {
  return absl::visit(
      [](const auto& alternative) -> absl::Span<const Type> {
        return alternative.parameters();
      },
      variant_);
}

bool operator==(const Type& lhs, const Type& rhs) {
  if (lhs.IsStruct() && rhs.IsStruct()) {
    return static_cast<StructType>(lhs) == static_cast<StructType>(rhs);
  } else if (lhs.IsStruct() || rhs.IsStruct()) {
    return false;
  } else {
    return lhs.variant_ == rhs.variant_;
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

bool Type::IsOptional() const {
  return IsOpaque() && static_cast<OpaqueType>(*this).IsOptional();
}

namespace {

template <typename T>
absl::optional<T> GetOrNullopt(const common_internal::TypeVariant& variant) {
  if (const auto* alt = absl::get_if<T>(&variant); alt != nullptr) {
    return *alt;
  }
  return absl::nullopt;
}

}  // namespace

absl::optional<AnyType> Type::AsAny() const {
  return GetOrNullopt<AnyType>(variant_);
}

absl::optional<BoolType> Type::AsBool() const {
  return GetOrNullopt<BoolType>(variant_);
}

absl::optional<BoolWrapperType> Type::AsBoolWrapper() const {
  return GetOrNullopt<BoolWrapperType>(variant_);
}

absl::optional<BytesType> Type::AsBytes() const {
  return GetOrNullopt<BytesType>(variant_);
}

absl::optional<BytesWrapperType> Type::AsBytesWrapper() const {
  return GetOrNullopt<BytesWrapperType>(variant_);
}

absl::optional<DoubleType> Type::AsDouble() const {
  return GetOrNullopt<DoubleType>(variant_);
}

absl::optional<DoubleWrapperType> Type::AsDoubleWrapper() const {
  return GetOrNullopt<DoubleWrapperType>(variant_);
}

absl::optional<DurationType> Type::AsDuration() const {
  return GetOrNullopt<DurationType>(variant_);
}

absl::optional<DynType> Type::AsDyn() const {
  return GetOrNullopt<DynType>(variant_);
}

absl::optional<EnumType> Type::AsEnum() const {
  return GetOrNullopt<EnumType>(variant_);
}

absl::optional<ErrorType> Type::AsError() const {
  return GetOrNullopt<ErrorType>(variant_);
}

absl::optional<FunctionType> Type::AsFunction() const {
  return GetOrNullopt<FunctionType>(variant_);
}

absl::optional<IntType> Type::AsInt() const {
  return GetOrNullopt<IntType>(variant_);
}

absl::optional<IntWrapperType> Type::AsIntWrapper() const {
  return GetOrNullopt<IntWrapperType>(variant_);
}

absl::optional<ListType> Type::AsList() const {
  return GetOrNullopt<ListType>(variant_);
}

absl::optional<MapType> Type::AsMap() const {
  return GetOrNullopt<MapType>(variant_);
}

absl::optional<MessageType> Type::AsMessage() const {
  return GetOrNullopt<MessageType>(variant_);
}

absl::optional<NullType> Type::AsNull() const {
  return GetOrNullopt<NullType>(variant_);
}

absl::optional<OpaqueType> Type::AsOpaque() const {
  return GetOrNullopt<OpaqueType>(variant_);
}

absl::optional<OptionalType> Type::AsOptional() const {
  if (auto maybe_opaque = AsOpaque(); maybe_opaque.has_value()) {
    return maybe_opaque->AsOptional();
  }
  return absl::nullopt;
}

absl::optional<StringType> Type::AsString() const {
  return GetOrNullopt<StringType>(variant_);
}

absl::optional<StringWrapperType> Type::AsStringWrapper() const {
  return GetOrNullopt<StringWrapperType>(variant_);
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

absl::optional<TimestampType> Type::AsTimestamp() const {
  return GetOrNullopt<TimestampType>(variant_);
}

absl::optional<TypeParamType> Type::AsTypeParam() const {
  return GetOrNullopt<TypeParamType>(variant_);
}

absl::optional<TypeType> Type::AsType() const {
  return GetOrNullopt<TypeType>(variant_);
}

absl::optional<UintType> Type::AsUint() const {
  return GetOrNullopt<UintType>(variant_);
}

absl::optional<UintWrapperType> Type::AsUintWrapper() const {
  return GetOrNullopt<UintWrapperType>(variant_);
}

absl::optional<UnknownType> Type::AsUnknown() const {
  return GetOrNullopt<UnknownType>(variant_);
}

namespace {

template <typename T>
T GetOrDie(const common_internal::TypeVariant& variant) {
  return absl::get<T>(variant);
}

}  // namespace

Type::operator AnyType() const {
  ABSL_DCHECK(IsAny()) << DebugString();
  return GetOrDie<AnyType>(variant_);
}

Type::operator BoolType() const {
  ABSL_DCHECK(IsBool()) << DebugString();
  return GetOrDie<BoolType>(variant_);
}

Type::operator BoolWrapperType() const {
  ABSL_DCHECK(IsBoolWrapper()) << DebugString();
  return GetOrDie<BoolWrapperType>(variant_);
}

Type::operator BytesType() const {
  ABSL_DCHECK(IsBytes()) << DebugString();
  return GetOrDie<BytesType>(variant_);
}

Type::operator BytesWrapperType() const {
  ABSL_DCHECK(IsBytesWrapper()) << DebugString();
  return GetOrDie<BytesWrapperType>(variant_);
}

Type::operator DoubleType() const {
  ABSL_DCHECK(IsDouble()) << DebugString();
  return GetOrDie<DoubleType>(variant_);
}

Type::operator DoubleWrapperType() const {
  ABSL_DCHECK(IsDoubleWrapper()) << DebugString();
  return GetOrDie<DoubleWrapperType>(variant_);
}

Type::operator DurationType() const {
  ABSL_DCHECK(IsDuration()) << DebugString();
  return GetOrDie<DurationType>(variant_);
}

Type::operator DynType() const {
  ABSL_DCHECK(IsDyn()) << DebugString();
  return GetOrDie<DynType>(variant_);
}

Type::operator EnumType() const {
  ABSL_DCHECK(IsEnum()) << DebugString();
  return GetOrDie<EnumType>(variant_);
}

Type::operator ErrorType() const {
  ABSL_DCHECK(IsError()) << DebugString();
  return GetOrDie<ErrorType>(variant_);
}

Type::operator FunctionType() const {
  ABSL_DCHECK(IsFunction()) << DebugString();
  return GetOrDie<FunctionType>(variant_);
}

Type::operator IntType() const {
  ABSL_DCHECK(IsInt()) << DebugString();
  return GetOrDie<IntType>(variant_);
}

Type::operator IntWrapperType() const {
  ABSL_DCHECK(IsIntWrapper()) << DebugString();
  return GetOrDie<IntWrapperType>(variant_);
}

Type::operator ListType() const {
  ABSL_DCHECK(IsList()) << DebugString();
  return GetOrDie<ListType>(variant_);
}

Type::operator MapType() const {
  ABSL_DCHECK(IsMap()) << DebugString();
  return GetOrDie<MapType>(variant_);
}

Type::operator MessageType() const {
  ABSL_DCHECK(IsMessage()) << DebugString();
  return GetOrDie<MessageType>(variant_);
}

Type::operator NullType() const {
  ABSL_DCHECK(IsNull()) << DebugString();
  return GetOrDie<NullType>(variant_);
}

Type::operator OpaqueType() const {
  ABSL_DCHECK(IsOpaque()) << DebugString();
  return GetOrDie<OpaqueType>(variant_);
}

Type::operator OptionalType() const {
  ABSL_DCHECK(IsOptional()) << DebugString();
  return static_cast<OptionalType>(GetOrDie<OpaqueType>(variant_));
}

Type::operator StringType() const {
  ABSL_DCHECK(IsString()) << DebugString();
  return GetOrDie<StringType>(variant_);
}

Type::operator StringWrapperType() const {
  ABSL_DCHECK(IsStringWrapper()) << DebugString();
  return GetOrDie<StringWrapperType>(variant_);
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

Type::operator TimestampType() const {
  ABSL_DCHECK(IsTimestamp()) << DebugString();
  return GetOrDie<TimestampType>(variant_);
}

Type::operator TypeParamType() const {
  ABSL_DCHECK(IsTypeParam()) << DebugString();
  return GetOrDie<TypeParamType>(variant_);
}

Type::operator TypeType() const {
  ABSL_DCHECK(IsType()) << DebugString();
  return GetOrDie<TypeType>(variant_);
}

Type::operator UintType() const {
  ABSL_DCHECK(IsUint()) << DebugString();
  return GetOrDie<UintType>(variant_);
}

Type::operator UintWrapperType() const {
  ABSL_DCHECK(IsUintWrapper()) << DebugString();
  return GetOrDie<UintWrapperType>(variant_);
}

Type::operator UnknownType() const {
  ABSL_DCHECK(IsUnknown()) << DebugString();
  return GetOrDie<UnknownType>(variant_);
}

}  // namespace cel
