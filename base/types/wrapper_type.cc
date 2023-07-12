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

#include "base/types/wrapper_type.h"

#include <utility>

#include "absl/base/optimization.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/value_factory.h"
#include "internal/proto_wire.h"

namespace cel {

namespace {

using internal::MakeProtoWireTag;
using internal::ProtoWireDecoder;
using internal::ProtoWireType;

}  // namespace

template class Handle<WrapperType>;
template class Handle<BoolWrapperType>;
template class Handle<BytesWrapperType>;
template class Handle<DoubleWrapperType>;
template class Handle<IntWrapperType>;
template class Handle<StringWrapperType>;
template class Handle<UintWrapperType>;

absl::string_view WrapperType::name() const {
  switch (base_internal::Metadata::GetInlineVariant<Kind>(*this)) {
    case Kind::kBool:
      return static_cast<const BoolWrapperType*>(this)->name();
    case Kind::kBytes:
      return static_cast<const BytesWrapperType*>(this)->name();
    case Kind::kDouble:
      return static_cast<const DoubleWrapperType*>(this)->name();
    case Kind::kInt:
      return static_cast<const IntWrapperType*>(this)->name();
    case Kind::kString:
      return static_cast<const StringWrapperType*>(this)->name();
    case Kind::kUint:
      return static_cast<const UintWrapperType*>(this)->name();
    default:
      // There are only 6 wrapper types.
      ABSL_UNREACHABLE();
  }
}

absl::StatusOr<Handle<Value>> WrapperType::NewValueFromAny(
    ValueFactory& value_factory, const absl::Cord& value) const {
  switch (base_internal::Metadata::GetInlineVariant<Kind>(*this)) {
    case Kind::kBool:
      return static_cast<const BoolWrapperType*>(this)->NewValueFromAny(
          value_factory, value);
    case Kind::kBytes:
      return static_cast<const BytesWrapperType*>(this)->NewValueFromAny(
          value_factory, value);
    case Kind::kDouble:
      return static_cast<const DoubleWrapperType*>(this)->NewValueFromAny(
          value_factory, value);
    case Kind::kInt:
      return static_cast<const IntWrapperType*>(this)->NewValueFromAny(
          value_factory, value);
    case Kind::kString:
      return static_cast<const StringWrapperType*>(this)->NewValueFromAny(
          value_factory, value);
    case Kind::kUint:
      return static_cast<const UintWrapperType*>(this)->NewValueFromAny(
          value_factory, value);
    default:
      // There are only 6 wrapper types.
      ABSL_UNREACHABLE();
  }
}

absl::Span<const absl::string_view> WrapperType::aliases() const {
  switch (base_internal::Metadata::GetInlineVariant<Kind>(*this)) {
    case Kind::kDouble:
      return static_cast<const DoubleWrapperType*>(this)->aliases();
    case Kind::kInt:
      return static_cast<const IntWrapperType*>(this)->aliases();
    case Kind::kUint:
      return static_cast<const UintWrapperType*>(this)->aliases();
    default:
      // The other wrappers do not have aliases.
      return absl::Span<const absl::string_view>();
  }
}

const Handle<Type>& WrapperType::wrapped() const {
  switch (base_internal::Metadata::GetInlineVariant<Kind>(*this)) {
    case Kind::kBool:
      return static_cast<const BoolWrapperType*>(this)->wrapped().As<Type>();
    case Kind::kBytes:
      return static_cast<const BytesWrapperType*>(this)->wrapped().As<Type>();
    case Kind::kDouble:
      return static_cast<const DoubleWrapperType*>(this)->wrapped().As<Type>();
    case Kind::kInt:
      return static_cast<const IntWrapperType*>(this)->wrapped().As<Type>();
    case Kind::kString:
      return static_cast<const StringWrapperType*>(this)->wrapped().As<Type>();
    case Kind::kUint:
      return static_cast<const UintWrapperType*>(this)->wrapped().As<Type>();
    default:
      // There are only 6 wrapper types.
      ABSL_UNREACHABLE();
  }
}

absl::Span<const absl::string_view> DoubleWrapperType::aliases() const {
  static constexpr absl::string_view kAliases[] = {
      "google.protobuf.FloatValue"};
  return absl::MakeConstSpan(kAliases);
}

absl::Span<const absl::string_view> IntWrapperType::aliases() const {
  static constexpr absl::string_view kAliases[] = {
      "google.protobuf.Int32Value"};
  return absl::MakeConstSpan(kAliases);
}

absl::Span<const absl::string_view> UintWrapperType::aliases() const {
  static constexpr absl::string_view kAliases[] = {
      "google.protobuf.UInt32Value"};
  return absl::MakeConstSpan(kAliases);
}

absl::StatusOr<Handle<BoolValue>> BoolWrapperType::NewValueFromAny(
    ValueFactory& value_factory, const absl::Cord& value) const {
  // google.protobuf.BoolValue.
  bool primitive = false;
  ProtoWireDecoder decoder("google.protobuf.BoolValue", value);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kVarint)) {
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadVarint<bool>());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return value_factory.CreateBoolValue(primitive);
}

absl::StatusOr<Handle<DoubleValue>> DoubleWrapperType::NewValueFromAny(
    ValueFactory& value_factory, const absl::Cord& value) const {
  // google.protobuf.{FloatValue,DoubleValue}.
  double primitive = 0.0;
  ProtoWireDecoder decoder("google.protobuf.DoubleValue", value);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kFixed64)) {
      // google.protobuf.DoubleValue
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadFixed64<double>());
      continue;
    }
    if (tag == MakeProtoWireTag(1, ProtoWireType::kFixed32)) {
      // google.protobuf.FloatValue
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadFixed32<float>());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return value_factory.CreateDoubleValue(primitive);
}

absl::StatusOr<Handle<IntValue>> IntWrapperType::NewValueFromAny(
    ValueFactory& value_factory, const absl::Cord& value) const {
  // google.protobuf.{Int32Value,Int64Value}
  int64_t primitive = 0;
  ProtoWireDecoder decoder("google.protobuf.Int64Value", value);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kVarint)) {
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadVarint<int64_t>());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return value_factory.CreateIntValue(primitive);
}

absl::StatusOr<Handle<UintValue>> UintWrapperType::NewValueFromAny(
    ValueFactory& value_factory, const absl::Cord& value) const {
  // google.protobuf.{UInt32Value,UInt64Value}
  uint64_t primitive = 0;
  ProtoWireDecoder decoder("google.protobuf.UInt64Value", value);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kVarint)) {
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadVarint<uint64_t>());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return value_factory.CreateUintValue(primitive);
}

absl::StatusOr<Handle<BytesValue>> BytesWrapperType::NewValueFromAny(
    ValueFactory& value_factory, const absl::Cord& value) const {
  // google.protobuf.BytesValue
  Handle<BytesValue> primitive = value_factory.GetBytesValue();
  ProtoWireDecoder decoder("google.protobuf.BytesValue", value);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kLengthDelimited)) {
      CEL_ASSIGN_OR_RETURN(auto primitive_value, decoder.ReadLengthDelimited());
      CEL_ASSIGN_OR_RETURN(primitive, value_factory.CreateBytesValue(
                                          std::move(primitive_value)));
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return primitive;
}

absl::StatusOr<Handle<StringValue>> StringWrapperType::NewValueFromAny(
    ValueFactory& value_factory, const absl::Cord& value) const {
  // google.protobuf.StringValue
  Handle<StringValue> primitive = value_factory.GetStringValue();
  ProtoWireDecoder decoder("google.protobuf.StringValue", value);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kLengthDelimited)) {
      CEL_ASSIGN_OR_RETURN(auto primitive_value, decoder.ReadLengthDelimited());
      CEL_ASSIGN_OR_RETURN(primitive, value_factory.CreateStringValue(
                                          std::move(primitive_value)));
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return primitive;
}

}  // namespace cel
