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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_CEL_PROTO_LITE_WRAP_UTIL_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_CEL_PROTO_LITE_WRAP_UTIL_H_

#include <string>
#include <type_traits>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "google/protobuf/arena.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "eval/public/cel_value.h"

namespace google::api::expr::runtime::internal {

CelValue CreateCelValue(bool value, const LegacyTypeInfoApis* type_info,
                        google::protobuf::Arena* arena);
CelValue CreateCelValue(int32_t value, const LegacyTypeInfoApis* type_info,
                        google::protobuf::Arena* arena);
CelValue CreateCelValue(int64_t value, const LegacyTypeInfoApis* type_info,
                        google::protobuf::Arena* arena);
CelValue CreateCelValue(uint32_t value, const LegacyTypeInfoApis* type_info,
                        google::protobuf::Arena* arena);
CelValue CreateCelValue(uint64_t value, const LegacyTypeInfoApis* type_info,
                        google::protobuf::Arena* arena);
CelValue CreateCelValue(float value, const LegacyTypeInfoApis* type_info,
                        google::protobuf::Arena* arena);
CelValue CreateCelValue(double value, const LegacyTypeInfoApis* type_info,
                        google::protobuf::Arena* arena);
// Creates CelValue from provided std::string.
CelValue CreateCelValue(const std::string& value,
                        const LegacyTypeInfoApis* type_info,
                        google::protobuf::Arena* arena);
// Creates CelValue from provided absl::Cord.
CelValue CreateCelValue(const absl::Cord& value,
                        const LegacyTypeInfoApis* type_info,
                        google::protobuf::Arena* arena);
// Creates CelValue from provided google::protobuf::BoolValue.
CelValue CreateCelValue(const google::protobuf::BoolValue& wrapper,
                        const LegacyTypeInfoApis* type_info,
                        google::protobuf::Arena* arena);
// Creates CelValue from provided google::protobuf::Duration.
CelValue CreateCelValue(const google::protobuf::Duration& duration,
                        const LegacyTypeInfoApis* type_info,
                        google::protobuf::Arena* arena);
// Creates CelValue from provided google::protobuf::Timestamp.
CelValue CreateCelValue(const google::protobuf::Timestamp& timestamp,
                        const LegacyTypeInfoApis* type_info,
                        google::protobuf::Arena* arena);
// Creates CelValue from provided std::string.
CelValue CreateCelValue(const std::string& value,
                        const LegacyTypeInfoApis* type_info,
                        google::protobuf::Arena* arena);
// Creates CelValue from provided google::protobuf::Int32Value.
CelValue CreateCelValue(const google::protobuf::Int32Value& wrapper,
                        const LegacyTypeInfoApis* type_info,
                        google::protobuf::Arena* arena);
// Creates CelValue from provided google::protobuf::Int64Value.
CelValue CreateCelValue(const google::protobuf::Int64Value& wrapper,
                        const LegacyTypeInfoApis* type_info,
                        google::protobuf::Arena* arena);
// Creates CelValue from provided google::protobuf::UInt32Value.
CelValue CreateCelValue(const google::protobuf::UInt32Value& wrapper,
                        const LegacyTypeInfoApis* type_info,
                        google::protobuf::Arena* arena);
// Creates CelValue from provided google::protobuf::UInt64Value.
CelValue CreateCelValue(const google::protobuf::UInt64Value& wrapper,
                        const LegacyTypeInfoApis* type_info,
                        google::protobuf::Arena* arena);
// Creates CelValue from provided google::protobuf::FloatValue.
CelValue CreateCelValue(const google::protobuf::FloatValue& wrapper,
                        const LegacyTypeInfoApis* type_info,
                        google::protobuf::Arena* arena);
// Creates CelValue from provided google::protobuf::DoubleValue.
CelValue CreateCelValue(const google::protobuf::DoubleValue& wrapper,
                        const LegacyTypeInfoApis* type_info,
                        google::protobuf::Arena* arena);
// Creates CelValue from provided google::protobuf::Value.
CelValue CreateCelValue(const google::protobuf::Value& value,
                        const LegacyTypeInfoApis* type_info,
                        google::protobuf::Arena* arena);
// Creates CelValue from provided google::protobuf::ListValue.
CelValue CreateCelValue(const google::protobuf::ListValue& list_value,
                        const LegacyTypeInfoApis* type_info,
                        google::protobuf::Arena* arena);
// Creates CelValue from provided google::protobuf::Struct.
CelValue CreateCelValue(const google::protobuf::Struct& struct_value,
                        const LegacyTypeInfoApis* type_info,
                        google::protobuf::Arena* arena);
// Creates CelValue from provided google::protobuf::StringValue.
CelValue CreateCelValue(const google::protobuf::StringValue& wrapper,
                        const LegacyTypeInfoApis* type_info,
                        google::protobuf::Arena* arena);
// Creates CelValue from provided google::protobuf::BytesValue.
CelValue CreateCelValue(const google::protobuf::BytesValue& wrapper,
                        const LegacyTypeInfoApis* type_info,
                        google::protobuf::Arena* arena);
// Creates CelValue from provided google::protobuf::Any.
CelValue CreateCelValue(const google::protobuf::Any& any_value,
                        const LegacyTypeInfoApis* type_info,
                        google::protobuf::Arena* arena);
// Creates CelValue from provided MessageLite-derived typed reference. It always
// created MessageWrapper CelValue, since this function should be matching
// non-well known type.
template <typename T>
inline CelValue CreateCelValue(const T& message,
                               const LegacyTypeInfoApis* type_info,
                               google::protobuf::Arena* arena) {
  static_assert(!std::is_base_of_v<google::protobuf::MessageLite, T*>,
                "Call to templated version of CreateCelValue with "
                "non-MessageLite derived type name. Please specialize the "
                "implementation to support this new type.");
  return CelValue::CreateMessageWrapper(
      CelValue::MessageWrapper(&message, type_info));
}
// Throws compilation error, since creation of CelValue from provided a pointer
// is not supported.
template <typename T>
inline CelValue CreateCelValue(const T* message_pointer,
                               const LegacyTypeInfoApis* type_info,
                               google::protobuf::Arena* arena) {
  // We don't allow calling this function with a pointer, since all of the
  // relevant proto functions return references.
  static_assert(
      !std::is_base_of_v<google::protobuf::MessageLite, T> &&
          !std::is_same_v<google::protobuf::MessageLite, T>,
      "Call to CreateCelValue with MessageLite pointer is not allowed. Please "
      "call this function with a reference to the object.");
  static_assert(
      std::is_base_of_v<google::protobuf::MessageLite, T>,
      "Call to CreateCelValue with a pointer is not "
      "allowed. Try calling this function with a reference to the object.");
  return CreateErrorValue(arena,
                          "Unintended call to CreateCelValue "
                          "with a pointer.");
}

// Create CelValue by unwrapping message provided by google::protobuf::MessageLite to a
// well known type. If the type is not well known, returns absl::NotFound error.
absl::StatusOr<CelValue> UnwrapFromWellKnownType(
    const google::protobuf::MessageLite* message, const LegacyTypeInfoApis* type_info,
    google::protobuf::Arena* arena);

// Creates message of type google::protobuf::DoubleValue from provided
// 'cel_value'. If provided 'wrapper' is nullptr, allocates new message in the
// provided 'arena'.
absl::StatusOr<google::protobuf::DoubleValue*> CreateMessageFromValue(
    const CelValue& cel_value, google::protobuf::DoubleValue* wrapper,
    google::protobuf::Arena* arena);
// Creates message of type google::protobuf::FloatValue from provided
// 'cel_value'. If provided 'wrapper' is nullptr, allocates new message in the
// provided 'arena'.
absl::StatusOr<google::protobuf::FloatValue*> CreateMessageFromValue(
    const CelValue& cel_value, google::protobuf::FloatValue* wrapper,
    google::protobuf::Arena* arena);
// Creates message of type google::protobuf::Int32Value from provided
// 'cel_value'. If provided 'wrapper' is nullptr, allocates new message in the
// provided 'arena'.
absl::StatusOr<google::protobuf::Int32Value*> CreateMessageFromValue(
    const CelValue& cel_value, google::protobuf::Int32Value* wrapper,
    google::protobuf::Arena* arena);
// Creates message of type google::protobuf::UInt32Value from provided
// 'cel_value'. If provided 'wrapper' is nullptr, allocates new message in the
// provided 'arena'.
absl::StatusOr<google::protobuf::UInt32Value*> CreateMessageFromValue(
    const CelValue& cel_value, google::protobuf::UInt32Value* wrapper,
    google::protobuf::Arena* arena);
// Creates message of type google::protobuf::Int64Value from provided
// 'cel_value'. If provided 'wrapper' is nullptr, allocates new message in the
// provided 'arena'.
absl::StatusOr<google::protobuf::Int64Value*> CreateMessageFromValue(
    const CelValue& cel_value, google::protobuf::Int64Value* wrapper,
    google::protobuf::Arena* arena);
// Creates message of type google::protobuf::UInt64Value from provided
// 'cel_value'. If provided 'wrapper' is nullptr, allocates new message in the
// provided 'arena'.
absl::StatusOr<google::protobuf::UInt64Value*> CreateMessageFromValue(
    const CelValue& cel_value, google::protobuf::UInt64Value* wrapper,
    google::protobuf::Arena* arena);
// Creates message of type google::protobuf::StringValue from provided
// 'cel_value'. If provided 'wrapper' is nullptr, allocates new message in the
// provided 'arena'.
absl::StatusOr<google::protobuf::StringValue*> CreateMessageFromValue(
    const CelValue& cel_value, google::protobuf::StringValue* wrapper,
    google::protobuf::Arena* arena);
// Creates message of type google::protobuf::BytesValue from provided
// 'cel_value'. If provided 'wrapper' is nullptr, allocates new message in the
// provided 'arena'.
absl::StatusOr<google::protobuf::BytesValue*> CreateMessageFromValue(
    const CelValue& cel_value, google::protobuf::BytesValue* wrapper,
    google::protobuf::Arena* arena);
// Creates message of type google::protobuf::BoolValue from provided
// 'cel_value'. If provided 'wrapper' is nullptr, allocates new message in the
// provided 'arena'.
absl::StatusOr<google::protobuf::BoolValue*> CreateMessageFromValue(
    const CelValue& cel_value, google::protobuf::BoolValue* wrapper,
    google::protobuf::Arena* arena);
// Creates message of type google::protobuf::Any from provided 'cel_value'. If
// provided 'wrapper' is nullptr, allocates new message in the provided 'arena'.
absl::StatusOr<google::protobuf::Any*> CreateMessageFromValue(
    const CelValue& cel_value, google::protobuf::Any* wrapper,
    google::protobuf::Arena* arena);
// Creates message of type google::protobuf::Duration from provided 'cel_value'.
// If provided 'wrapper' is nullptr, allocates new message in the provided
// 'arena'.
absl::StatusOr<google::protobuf::Duration*> CreateMessageFromValue(
    const CelValue& cel_value, google::protobuf::Duration* wrapper,
    google::protobuf::Arena* arena);
// Creates message of type <::google::protobuf::Timestamp from provided
// 'cel_value'. If provided 'wrapper' is nullptr, allocates new message in the
// provided 'arena'.
absl::StatusOr<::google::protobuf::Timestamp*> CreateMessageFromValue(
    const CelValue& cel_value, ::google::protobuf::Timestamp* wrapper,
    google::protobuf::Arena* arena);
// Creates message of type google::protobuf::Value from provided 'cel_value'. If
// provided 'wrapper' is nullptr, allocates new message in the provided 'arena'.
absl::StatusOr<google::protobuf::Value*> CreateMessageFromValue(
    const CelValue& cel_value, google::protobuf::Value* wrapper,
    google::protobuf::Arena* arena);
// Creates message of type google::protobuf::ListValue from provided
// 'cel_value'. If provided 'wrapper' is nullptr, allocates new message in the
// provided 'arena'.
absl::StatusOr<google::protobuf::ListValue*> CreateMessageFromValue(
    const CelValue& cel_value, google::protobuf::ListValue* wrapper,
    google::protobuf::Arena* arena);
// Creates message of type google::protobuf::Struct from provided 'cel_value'.
// If provided 'wrapper' is nullptr, allocates new message in the provided
// 'arena'.
absl::StatusOr<google::protobuf::Struct*> CreateMessageFromValue(
    const CelValue& cel_value, google::protobuf::Struct* wrapper,
    google::protobuf::Arena* arena);
// Creates message of type google::protobuf::StringValue from provided
// 'cel_value'. If provided 'wrapper' is nullptr, allocates new message in the
// provided 'arena'.
absl::StatusOr<google::protobuf::StringValue*> CreateMessageFromValue(
    const CelValue& cel_value, google::protobuf::StringValue* wrapper,
    google::protobuf::Arena* arena);
// Creates message of type google::protobuf::BytesValue from provided
// 'cel_value'. If provided 'wrapper' is nullptr, allocates new message in the
// provided 'arena'.
absl::StatusOr<google::protobuf::BytesValue*> CreateMessageFromValue(
    const CelValue& cel_value, google::protobuf::BytesValue* wrapper,
    google::protobuf::Arena* arena);
// Creates message of type google::protobuf::Any from provided 'cel_value'. If
// provided 'wrapper' is nullptr, allocates new message in the provided 'arena'.
absl::StatusOr<google::protobuf::Any*> CreateMessageFromValue(
    const CelValue& cel_value, google::protobuf::Any* wrapper,
    google::protobuf::Arena* arena);
// Returns Unimplemented for all non-matched message types.
template <typename T>
inline absl::StatusOr<T*> CreateMessageFromValue(const CelValue& cel_value,
                                                 T* wrapper,
                                                 google::protobuf::Arena* arena) {
  return absl::UnimplementedError("Not implemented");
}
}  // namespace google::api::expr::runtime::internal

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_CEL_PROTO_LITE_WRAP_UTIL_H_
