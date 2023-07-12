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

#include "base/types/any_type.h"

#include <string>

#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/strip.h"
#include "base/value_factory.h"
#include "internal/proto_wire.h"
#include "internal/status_macros.h"

namespace cel {

namespace {

using internal::MakeProtoWireTag;
using internal::ProtoWireDecoder;
using internal::ProtoWireType;

}  // namespace

CEL_INTERNAL_TYPE_IMPL(AnyType);

absl::StatusOr<Handle<Value>> AnyType::NewValueFromAny(
    ValueFactory& value_factory, const absl::Cord& value) const {
  // google.protobuf.Any
  std::string type_url;
  absl::Cord payload;
  ProtoWireDecoder decoder("google.protobuf.Any", value);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kLengthDelimited)) {
      CEL_ASSIGN_OR_RETURN(auto type_url_value, decoder.ReadLengthDelimited());
      type_url = static_cast<std::string>(type_url_value);
      continue;
    }
    if (tag == MakeProtoWireTag(2, ProtoWireType::kLengthDelimited)) {
      CEL_ASSIGN_OR_RETURN(payload, decoder.ReadLengthDelimited());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  CEL_ASSIGN_OR_RETURN(
      auto type, value_factory.type_manager().ResolveType(
                     absl::StripPrefix(type_url, "type.googleapis.com/")));
  if (ABSL_PREDICT_FALSE(!type.has_value())) {
    return absl::NotFoundError(absl::StrCat(
        "unable to find type to deserialize google.protobuf.Any: ", type_url));
  }
  return (*type)->NewValueFromAny(value_factory, payload);
}

}  // namespace cel
