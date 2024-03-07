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

#include "common/json.h"

#include <string>
#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/functional/overload.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "common/any.h"
#include "internal/copy_on_write.h"
#include "internal/proto_wire.h"
#include "internal/status_macros.h"

namespace cel {

internal::CopyOnWrite<typename JsonArray::Container> JsonArray::Empty() {
  static const absl::NoDestructor<internal::CopyOnWrite<Container>> empty;
  return *empty;
}

internal::CopyOnWrite<typename JsonObject::Container> JsonObject::Empty() {
  static const absl::NoDestructor<internal::CopyOnWrite<Container>> empty;
  return *empty;
}

Json JsonInt(int64_t value) {
  if (value < kJsonMinInt || value > kJsonMaxInt) {
    return JsonString(absl::StrCat(value));
  }
  return Json(static_cast<double>(value));
}

Json JsonUint(uint64_t value) {
  if (value > kJsonMaxUint) {
    return JsonString(absl::StrCat(value));
  }
  return Json(static_cast<double>(value));
}

Json JsonBytes(absl::string_view value) {
  return JsonString(absl::Base64Escape(value));
}

Json JsonBytes(const absl::Cord& value) {
  if (auto flat = value.TryFlat(); flat.has_value()) {
    return JsonBytes(*flat);
  }
  return JsonBytes(absl::string_view(static_cast<std::string>(value)));
}

bool JsonArrayBuilder::empty() const { return impl_.get().empty(); }

bool JsonArray::empty() const { return impl_.get().empty(); }

JsonArray::JsonArray(internal::CopyOnWrite<Container> impl)
    : impl_(std::move(impl)) {
  if (impl_.get().empty()) {
    impl_ = Empty();
  }
}

namespace {

using internal::ProtoWireEncoder;
using internal::ProtoWireTag;
using internal::ProtoWireType;

inline constexpr absl::string_view kJsonTypeName = "google.protobuf.Value";
inline constexpr absl::string_view kJsonArrayTypeName =
    "google.protobuf.ListValue";
inline constexpr absl::string_view kJsonObjectTypeName =
    "google.protobuf.Struct";

inline constexpr ProtoWireTag kValueNullValueFieldTag =
    ProtoWireTag(1, ProtoWireType::kVarint);
inline constexpr ProtoWireTag kValueBoolValueFieldTag =
    ProtoWireTag(4, ProtoWireType::kVarint);
inline constexpr ProtoWireTag kValueNumberValueFieldTag =
    ProtoWireTag(2, ProtoWireType::kFixed64);
inline constexpr ProtoWireTag kValueStringValueFieldTag =
    ProtoWireTag(3, ProtoWireType::kLengthDelimited);
inline constexpr ProtoWireTag kValueListValueFieldTag =
    ProtoWireTag(6, ProtoWireType::kLengthDelimited);
inline constexpr ProtoWireTag kValueStructValueFieldTag =
    ProtoWireTag(5, ProtoWireType::kLengthDelimited);

inline constexpr ProtoWireTag kListValueValuesFieldTag =
    ProtoWireTag(1, ProtoWireType::kLengthDelimited);

inline constexpr ProtoWireTag kStructFieldsEntryKeyFieldTag =
    ProtoWireTag(1, ProtoWireType::kLengthDelimited);
inline constexpr ProtoWireTag kStructFieldsEntryValueFieldTag =
    ProtoWireTag(2, ProtoWireType::kLengthDelimited);

absl::StatusOr<absl::Cord> JsonObjectEntryToAnyValue(const absl::Cord& key,
                                                     const Json& value) {
  absl::Cord data;
  ProtoWireEncoder encoder("google.protobuf.Struct.FieldsEntry", data);
  absl::Cord subdata;
  CEL_RETURN_IF_ERROR(JsonToAnyValue(value, subdata));
  CEL_RETURN_IF_ERROR(encoder.WriteTag(kStructFieldsEntryKeyFieldTag));
  CEL_RETURN_IF_ERROR(encoder.WriteLengthDelimited(std::move(key)));
  CEL_RETURN_IF_ERROR(encoder.WriteTag(kStructFieldsEntryValueFieldTag));
  CEL_RETURN_IF_ERROR(encoder.WriteLengthDelimited(std::move(subdata)));
  encoder.EnsureFullyEncoded();
  return data;
}

inline constexpr ProtoWireTag kStructFieldsFieldTag =
    ProtoWireTag(1, ProtoWireType::kLengthDelimited);

}  // namespace

absl::Status JsonToAnyValue(const Json& json, absl::Cord& data) {
  ProtoWireEncoder encoder(kJsonTypeName, data);
  absl::Status status = absl::visit(
      absl::Overload(
          [&encoder](JsonNull) -> absl::Status {
            CEL_RETURN_IF_ERROR(encoder.WriteTag(kValueNullValueFieldTag));
            return encoder.WriteVarint(0);
          },
          [&encoder](JsonBool value) -> absl::Status {
            CEL_RETURN_IF_ERROR(encoder.WriteTag(kValueBoolValueFieldTag));
            return encoder.WriteVarint(value);
          },
          [&encoder](JsonNumber value) -> absl::Status {
            CEL_RETURN_IF_ERROR(encoder.WriteTag(kValueNumberValueFieldTag));
            return encoder.WriteFixed64(value);
          },
          [&encoder](const JsonString& value) -> absl::Status {
            CEL_RETURN_IF_ERROR(encoder.WriteTag(kValueStringValueFieldTag));
            return encoder.WriteLengthDelimited(value);
          },
          [&encoder](const JsonArray& value) -> absl::Status {
            absl::Cord subdata;
            CEL_RETURN_IF_ERROR(JsonArrayToAnyValue(value, subdata));
            CEL_RETURN_IF_ERROR(encoder.WriteTag(kValueListValueFieldTag));
            return encoder.WriteLengthDelimited(std::move(subdata));
          },
          [&encoder](const JsonObject& value) -> absl::Status {
            absl::Cord subdata;
            CEL_RETURN_IF_ERROR(JsonObjectToAnyValue(value, subdata));
            CEL_RETURN_IF_ERROR(encoder.WriteTag(kValueStructValueFieldTag));
            return encoder.WriteLengthDelimited(std::move(subdata));
          }),
      json);
  CEL_RETURN_IF_ERROR(status);
  encoder.EnsureFullyEncoded();
  return absl::OkStatus();
}

absl::Status JsonArrayToAnyValue(const JsonArray& json, absl::Cord& data) {
  ProtoWireEncoder encoder(kJsonArrayTypeName, data);
  for (const auto& element : json) {
    absl::Cord subdata;
    CEL_RETURN_IF_ERROR(JsonToAnyValue(element, subdata));
    CEL_RETURN_IF_ERROR(encoder.WriteTag(kListValueValuesFieldTag));
    CEL_RETURN_IF_ERROR(encoder.WriteLengthDelimited(std::move(subdata)));
  }
  encoder.EnsureFullyEncoded();
  return absl::OkStatus();
}

absl::Status JsonObjectToAnyValue(const JsonObject& json, absl::Cord& data) {
  ProtoWireEncoder encoder(kJsonObjectTypeName, data);
  for (const auto& entry : json) {
    CEL_ASSIGN_OR_RETURN(auto subdata,
                         JsonObjectEntryToAnyValue(entry.first, entry.second));
    CEL_RETURN_IF_ERROR(encoder.WriteTag(kStructFieldsFieldTag));
    CEL_RETURN_IF_ERROR(encoder.WriteLengthDelimited(std::move(subdata)));
  }
  encoder.EnsureFullyEncoded();
  return absl::OkStatus();
}

absl::StatusOr<Any> JsonToAny(const Json& json) {
  absl::Cord data;
  CEL_RETURN_IF_ERROR(JsonToAnyValue(json, data));
  return MakeAny(MakeTypeUrl(kJsonTypeName), std::move(data));
}

absl::StatusOr<Any> JsonArrayToAny(const JsonArray& json) {
  absl::Cord data;
  CEL_RETURN_IF_ERROR(JsonArrayToAnyValue(json, data));
  return MakeAny(MakeTypeUrl(kJsonArrayTypeName), std::move(data));
}

absl::StatusOr<Any> JsonObjectToAny(const JsonObject& json) {
  absl::Cord data;
  CEL_RETURN_IF_ERROR(JsonObjectToAnyValue(json, data));
  return MakeAny(MakeTypeUrl(kJsonObjectTypeName), std::move(data));
}

}  // namespace cel
