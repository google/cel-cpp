#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_UTF8_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_UTF8_H_

#include <cstddef>
#include <utility>

#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"

namespace cel::internal {

// Returns true if the given UTF-8 encoded string is not malformed, false
// otherwise.
bool Utf8IsValid(absl::string_view str);
bool Utf8IsValid(const absl::Cord& str);

// Returns the number of Unicode code points in the UTF-8 encoded string.
//
// If there are any invalid bytes, they will each be counted as an invalid code
// point.
size_t Utf8CodePointCount(absl::string_view str);
size_t Utf8CodePointCount(const absl::Cord& str);

// Validates the given UTF-8 encoded string. The first return value is the
// number of code points and its meaning depends on the second return value. If
// the second return value is true the entire string is not malformed and the
// first return value is the number of code points. If the second return value
// is false the string is malformed and the first return value is the number of
// code points up until the malformed sequence was encountered.
std::pair<size_t, bool> Utf8Validate(absl::string_view str);
std::pair<size_t, bool> Utf8Validate(const absl::Cord& str);

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_UTF8_H_
