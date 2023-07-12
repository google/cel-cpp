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

#include "base/types/dyn_type.h"

#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/value_factory.h"
#include "internal/proto_wire.h"
#include "internal/status_macros.h"

namespace cel {

namespace {

using internal::MakeProtoWireTag;
using internal::ProtoWireDecoder;
using internal::ProtoWireType;

}  // namespace

CEL_INTERNAL_TYPE_IMPL(DynType);

absl::StatusOr<Handle<Value>> DynType::NewValueFromAny(
    ValueFactory& value_factory, const absl::Cord& value) const {
  // google.protobuf.Value.
  Handle<Value> primitive = value_factory.GetNullValue();
  ProtoWireDecoder decoder("google.protobuf.Value", value);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kVarint)) {
      CEL_ASSIGN_OR_RETURN(auto unused, decoder.ReadVarint<bool>());
      static_cast<void>(unused);
      primitive = value_factory.GetNullValue();
      continue;
    }
    if (tag == MakeProtoWireTag(2, ProtoWireType::kFixed64)) {
      CEL_ASSIGN_OR_RETURN(auto number_value, decoder.ReadFixed64<double>());
      primitive = value_factory.CreateDoubleValue(number_value);
      continue;
    }
    if (tag == MakeProtoWireTag(3, ProtoWireType::kLengthDelimited)) {
      CEL_ASSIGN_OR_RETURN(auto string_value, decoder.ReadLengthDelimited());
      CEL_ASSIGN_OR_RETURN(primitive,
                           value_factory.CreateStringValue(string_value));
      continue;
    }
    if (tag == MakeProtoWireTag(4, ProtoWireType::kVarint)) {
      CEL_ASSIGN_OR_RETURN(auto bool_value, decoder.ReadVarint<bool>());
      primitive = value_factory.CreateBoolValue(bool_value);
      continue;
    }
    if (tag == MakeProtoWireTag(5, ProtoWireType::kLengthDelimited)) {
      CEL_ASSIGN_OR_RETURN(auto struct_value, decoder.ReadLengthDelimited());
      CEL_ASSIGN_OR_RETURN(
          primitive,
          value_factory.type_factory().GetJsonMapType()->NewValueFromAny(
              value_factory, struct_value));
      continue;
    }
    if (tag == MakeProtoWireTag(6, ProtoWireType::kLengthDelimited)) {
      CEL_ASSIGN_OR_RETURN(auto list_value, decoder.ReadLengthDelimited());
      CEL_ASSIGN_OR_RETURN(
          primitive,
          value_factory.type_factory().GetJsonListType()->NewValueFromAny(
              value_factory, list_value));
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return primitive;
}

absl::Span<const absl::string_view> DynType::aliases() const {
  // Currently google.protobuf.Value also resolves to dyn.
  static constexpr absl::string_view kAliases[] = {"google.protobuf.Value"};
  return absl::MakeConstSpan(kAliases);
}

}  // namespace cel
