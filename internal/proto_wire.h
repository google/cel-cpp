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

// Utilities for decoding and encoding the protocol buffer wire format. CEL
// requires supporting `google.protobuf.Any`. The core of CEL cannot take a
// direct dependency on protobuf and utilities for encoding/decoding varint and
// fixed64 are not part of Abseil. So we either would have to either reject
// `google.protobuf.Any` when protobuf is not linked or implement the utilities
// ourselves. We chose the latter as it is the lesser of two evils and
// introduces significantly less complexity compared to the former.

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_PROTO_WIRE_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_PROTO_WIRE_H_

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/casts.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/numeric/bits.h"
#include "absl/strings/cord.h"
#include "absl/strings/cord_buffer.h"
#include "absl/strings/string_view.h"

namespace cel::internal {

// Calculates the number of bytes required to encode the unsigned integral `x`
// using varint.
template <typename T>
inline constexpr std::enable_if_t<
    (std::is_integral_v<T> && std::is_unsigned_v<T> && sizeof(T) <= 8), size_t>
VarintSize(T x) {
  return static_cast<size_t>(
      (static_cast<uint32_t>((sizeof(T) * 8 - 1) -
                             absl::countl_zero<T>(x | T{1})) *
           9 +
       73) /
      64);
}

// Overload of `VarintSize()` handling signed 64-bit integrals.
inline constexpr size_t VarintSize(int64_t x) {
  return VarintSize(static_cast<uint64_t>(x));
}

// Overload of `VarintSize()` handling signed 32-bit integrals.
inline constexpr size_t VarintSize(int32_t x) {
  // Sign-extend to 64-bits, then size.
  return VarintSize(static_cast<int64_t>(x));
}

// Overload of `VarintSize()` for bool.
inline constexpr size_t VarintSize(bool x ABSL_ATTRIBUTE_UNUSED) { return 1; }

// Compile-time constant for the size required to encode any value of the
// integral type `T` using varint.
template <typename T>
inline constexpr size_t kMaxVarintSize = VarintSize(static_cast<T>(~T{0}));

// Enumeration of the protocol buffer wire tags, see
// https://protobuf.dev/programming-guides/encoding/#structure.
enum class ProtoWireType : uint32_t {
  kVarint = 0,
  kFixed64 = 1,
  kLengthDelimited = 2,
  kStartGroup = 3,
  kEndGroup = 4,
  kFixed32 = 5,
};

// Creates the "tag" of a record, see
// https://protobuf.dev/programming-guides/encoding/#structure.
inline constexpr uint32_t MakeProtoWireTag(uint32_t field_number,
                                           ProtoWireType type) {
  ABSL_ASSERT(((field_number << 3) >> 3) == field_number);
  return (field_number << 3) | static_cast<uint32_t>(type);
}

// Encodes `value` as varint and stores it in `buffer`. This method should not
// be used outside of this header.
inline size_t VarintEncodeUnsafe(uint64_t value, char* buffer) {
  size_t length = 0;
  while (ABSL_PREDICT_FALSE(value >= 0x80)) {
    buffer[length++] = static_cast<char>(static_cast<uint8_t>(value | 0x80));
    value >>= 7;
  }
  buffer[length++] = static_cast<char>(static_cast<uint8_t>(value));
  return length;
}

// Encodes `value` as varint and appends it to `buffer`.
inline void VarintEncode(uint64_t value, absl::Cord& buffer) {
  // `absl::Cord::GetAppendBuffer` will allocate a block regardless of whether
  // `buffer` has enough inline storage space left. To take advantage of inline
  // storage space, we need to just do a plain append.
  char scratch[kMaxVarintSize<uint64_t>];
  buffer.Append(absl::string_view(scratch, VarintEncodeUnsafe(value, scratch)));
}

// Encodes `value` as varint and appends it to `buffer`.
inline void VarintEncode(int64_t value, absl::Cord& buffer) {
  return VarintEncode(absl::bit_cast<uint64_t>(value), buffer);
}

// Encodes `value` as varint and appends it to `buffer`.
inline void VarintEncode(uint32_t value, absl::Cord& buffer) {
  // `absl::Cord::GetAppendBuffer` will allocate a block regardless of whether
  // `buffer` has enough inline storage space left. To take advantage of inline
  // storage space, we need to just do a plain append.
  char scratch[kMaxVarintSize<uint32_t>];
  buffer.Append(absl::string_view(scratch, VarintEncodeUnsafe(value, scratch)));
}

// Encodes `value` as varint and appends it to `buffer`.
inline void VarintEncode(int32_t value, absl::Cord& buffer) {
  // Sign-extend to 64-bits, then encode.
  return VarintEncode(static_cast<int64_t>(value), buffer);
}

// Encodes `value` as varint and appends it to `buffer`.
inline void VarintEncode(bool value, absl::Cord& buffer) {
  // `absl::Cord::GetAppendBuffer` will allocate a block regardless of whether
  // `buffer` has enough inline storage space left. To take advantage of inline
  // storage space, we need to just do a plain append.
  char scratch = value ? char{1} : char{0};
  buffer.Append(absl::string_view(&scratch, 1));
}

inline void Fixed64EncodeUnsafe(uint64_t value, char* buffer) {
  buffer[0] = static_cast<char>(static_cast<uint8_t>(value));
  buffer[1] = static_cast<char>(static_cast<uint8_t>(value >> 8));
  buffer[2] = static_cast<char>(static_cast<uint8_t>(value >> 16));
  buffer[3] = static_cast<char>(static_cast<uint8_t>(value >> 24));
  buffer[4] = static_cast<char>(static_cast<uint8_t>(value >> 32));
  buffer[5] = static_cast<char>(static_cast<uint8_t>(value >> 40));
  buffer[6] = static_cast<char>(static_cast<uint8_t>(value >> 48));
  buffer[7] = static_cast<char>(static_cast<uint8_t>(value >> 56));
}

// Encodes `value` as a fixed-size number, see
// https://protobuf.dev/programming-guides/encoding/#non-varint-numbers.
inline void Fixed64Encode(uint64_t value, absl::Cord& buffer) {
  // `absl::Cord::GetAppendBuffer` will allocate a block regardless of whether
  // `buffer` has enough inline storage space left. To take advantage of inline
  // storage space, we need to just do a plain append.
  char scratch[8];
  Fixed64EncodeUnsafe(value, scratch);
  buffer.Append(absl::string_view(scratch, ABSL_ARRAYSIZE(scratch)));
}

// Encodes `value` as a fixed-size number, see
// https://protobuf.dev/programming-guides/encoding/#non-varint-numbers.
inline void Fixed64Encode(double value, absl::Cord& buffer) {
  Fixed64Encode(absl::bit_cast<uint64_t>(value), buffer);
}

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_PROTO_WIRE_H_
