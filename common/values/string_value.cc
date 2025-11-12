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

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/wrappers.pb.h"
#include "absl/base/nullability.h"
#include "absl/functional/overload.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/cord.h"
#include "absl/strings/cord_buffer.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/internal/byte_string.h"
#include "common/value.h"
#include "internal/status_macros.h"
#include "internal/strings.h"
#include "internal/utf8.h"
#include "internal/well_known_types.h"
#include "runtime/internal/errors.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

namespace {

using ::cel::well_known_types::ValueReflection;

template <typename Bytes>
std::string StringDebugString(const Bytes& value) {
  return value.NativeValue(absl::Overload(
      [](absl::string_view string) -> std::string {
        return internal::FormatStringLiteral(string);
      },
      [](const absl::Cord& cord) -> std::string {
        if (auto flat = cord.TryFlat(); flat.has_value()) {
          return internal::FormatStringLiteral(*flat);
        }
        return internal::FormatStringLiteral(static_cast<std::string>(cord));
      }));
}

}  // namespace

StringValue StringValue::Concat(const StringValue& lhs, const StringValue& rhs,
                                google::protobuf::Arena* absl_nonnull arena) {
  return StringValue(
      common_internal::ByteString::Concat(lhs.value_, rhs.value_, arena));
}

std::string StringValue::DebugString() const {
  return StringDebugString(*this);
}

absl::Status StringValue::SerializeTo(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(output != nullptr);

  google::protobuf::StringValue message;
  message.set_value(NativeString());
  if (!message.SerializePartialToZeroCopyStream(output)) {
    return absl::UnknownError(
        absl::StrCat("failed to serialize message: ", message.GetTypeName()));
  }

  return absl::OkStatus();
}

absl::Status StringValue::ConvertToJson(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE);

  ValueReflection value_reflection;
  CEL_RETURN_IF_ERROR(value_reflection.Initialize(json->GetDescriptor()));
  NativeValue(
      [&](const auto& value) { value_reflection.SetStringValue(json, value); });

  return absl::OkStatus();
}

absl::Status StringValue::Equal(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  if (auto other_value = other.AsString(); other_value.has_value()) {
    *result = NativeValue([other_value](const auto& value) -> BoolValue {
      return other_value->NativeValue(
          [&value](const auto& other_value) -> BoolValue {
            return BoolValue{value == other_value};
          });
    });
    return absl::OkStatus();
  }
  *result = FalseValue();
  return absl::OkStatus();
}

size_t StringValue::Size() const {
  return NativeValue([](const auto& alternative) -> size_t {
    return internal::Utf8CodePointCount(alternative);
  });
}

bool StringValue::IsEmpty() const {
  return NativeValue(
      [](const auto& alternative) -> bool { return alternative.empty(); });
}

bool StringValue::Equals(absl::string_view string) const {
  return value_.Equals(string);
}

bool StringValue::Equals(const absl::Cord& string) const {
  return value_.Equals(string);
}

bool StringValue::Equals(const StringValue& string) const {
  return value_.Equals(string.value_);
}

StringValue StringValue::Clone(google::protobuf::Arena* absl_nonnull arena) const {
  return StringValue(value_.Clone(arena));
}

int StringValue::Compare(absl::string_view string) const {
  return value_.Compare(string);
}

int StringValue::Compare(const absl::Cord& string) const {
  return value_.Compare(string);
}

int StringValue::Compare(const StringValue& string) const {
  return value_.Compare(string.value_);
}

bool StringValue::StartsWith(absl::string_view string) const {
  return value_.StartsWith(string);
}

bool StringValue::StartsWith(const absl::Cord& string) const {
  return value_.StartsWith(string);
}

bool StringValue::StartsWith(const StringValue& string) const {
  return value_.StartsWith(string.value_);
}

bool StringValue::EndsWith(absl::string_view string) const {
  return value_.EndsWith(string);
}

bool StringValue::EndsWith(const absl::Cord& string) const {
  return value_.EndsWith(string);
}

bool StringValue::EndsWith(const StringValue& string) const {
  return value_.EndsWith(string.value_);
}

bool StringValue::Contains(absl::string_view string) const {
  return value_.Visit(absl::Overload(
      [&](absl::string_view lhs) -> bool {
        return absl::StrContains(lhs, string);
      },
      [&](const absl::Cord& lhs) -> bool { return lhs.Contains(string); }));
}

bool StringValue::Contains(const absl::Cord& string) const {
  return value_.Visit(absl::Overload(
      [&](absl::string_view lhs) -> bool {
        if (auto flat = string.TryFlat(); flat) {
          return absl::StrContains(lhs, *flat);
        }
        // There is no nice way to do this. We cannot use std::search due to
        // absl::Cord::CharIterator being an input iterator instead of a forward
        // iterator. So just make an external cord with a noop releaser. We know
        // the external cord will not outlive this function.
        return absl::MakeCordFromExternal(lhs, []() {}).Contains(string);
      },
      [&](const absl::Cord& lhs) -> bool { return lhs.Contains(string); }));
}

bool StringValue::Contains(const StringValue& string) const {
  return string.value_.Visit(absl::Overload(
      [&](absl::string_view rhs) -> bool { return Contains(rhs); },
      [&](const absl::Cord& rhs) -> bool { return Contains(rhs); }));
}

namespace {

bool LowerAsciiImpl(absl::string_view in, std::string* absl_nonnull out) {
  if (in.empty()) {
    return false;
  }
  bool needs_conversion = false;
  for (char c : in) {
    if (absl::ascii_isupper(c)) {
      needs_conversion = true;
      break;
    }
  }

  if (!needs_conversion) {
    return false;
  }

  *out = absl::AsciiStrToLower(in);
  return true;
}

absl::Cord LowerAsciiImpl(const absl::Cord& in) {
  if (in.empty()) {
    return in;
  }
  size_t pos = 0;
  bool needs_conversion = false;
  for (char c : in.Chars()) {
    if (absl::ascii_isupper(c)) {
      needs_conversion = true;
      break;
    }
    pos++;
  }
  if (!needs_conversion) {
    return in;
  }
  absl::Cord out = in.Subcord(0, pos);
  absl::Cord rest = in.Subcord(pos, in.size() - pos);
  std::string suffix;
  suffix.resize(rest.size());
  size_t current = 0;
  for (char c : rest.Chars()) {
    suffix[current++] = absl::ascii_tolower(c);
  }
  out.Append(std::move(suffix));
  return out;
}

}  // namespace

StringValue StringValue::LowerAscii(google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(arena != nullptr);

  switch (value_.GetKind()) {
    case common_internal::ByteStringKind::kSmall: {
      std::string out;
      if (!(LowerAsciiImpl)(value_.GetSmall(), &out)) {
        return *this;
      }
      return StringValue::From(std::move(out), arena);
    }
    case common_internal::ByteStringKind::kMedium: {
      std::string out;
      if (!(LowerAsciiImpl)(value_.GetMedium(), &out)) {
        return *this;
      }
      return StringValue::From(std::move(out), arena);
    }
    case common_internal::ByteStringKind::kLarge:
      return StringValue::Wrap((LowerAsciiImpl)(value_.GetLarge()));
  }
}

namespace {

bool UpperAsciiImpl(absl::string_view in, std::string* absl_nonnull out) {
  if (in.empty()) {
    return false;
  }
  bool needs_conversion = false;
  for (char c : in) {
    if (absl::ascii_islower(c)) {
      needs_conversion = true;
      break;
    }
  }

  if (!needs_conversion) {
    return false;
  }

  *out = absl::AsciiStrToUpper(in);
  return true;
}

absl::Cord UpperAsciiImpl(const absl::Cord& in) {
  if (in.empty()) {
    return in;
  }
  size_t pos = 0;
  bool needs_conversion = false;
  for (char c : in.Chars()) {
    if (absl::ascii_islower(c)) {
      needs_conversion = true;
      break;
    }
    pos++;
  }
  if (!needs_conversion) {
    return in;
  }
  absl::Cord out = in.Subcord(0, pos);
  absl::Cord rest = in.Subcord(pos, in.size() - pos);
  std::string suffix;
  suffix.resize(rest.size());
  size_t current = 0;
  for (char c : rest.Chars()) {
    suffix[current++] = absl::ascii_toupper(c);
  }
  out.Append(std::move(suffix));
  return out;
}

}  // namespace

StringValue StringValue::UpperAscii(google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(arena != nullptr);

  switch (value_.GetKind()) {
    case common_internal::ByteStringKind::kSmall: {
      std::string out;
      if (!(UpperAsciiImpl)(value_.GetSmall(), &out)) {
        return *this;
      }
      return StringValue::From(std::move(out), arena);
    }
    case common_internal::ByteStringKind::kMedium: {
      std::string out;
      if (!(UpperAsciiImpl)(value_.GetMedium(), &out)) {
        return *this;
      }
      return StringValue::From(std::move(out), arena);
    }
    case common_internal::ByteStringKind::kLarge:
      return StringValue::Wrap((UpperAsciiImpl)(value_.GetLarge()));
  }
}

absl::Status StringValue::Join(
    const ListValue& list,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  std::string joined;

  CEL_ASSIGN_OR_RETURN(auto iterator, list.NewIterator());

  CEL_ASSIGN_OR_RETURN(
      absl::optional<Value> element,
      iterator->Next1(descriptor_pool, message_factory, arena));
  if (element) {
    if (auto string_element = element->AsString(); string_element) {
      string_element->AppendToString(&joined);
    } else {
      ABSL_DCHECK(!element->Is<ErrorValue>());
      *result =
          ErrorValue(runtime_internal::CreateNoMatchingOverloadError("join"));
      return absl::OkStatus();
    }
    while (true) {
      CEL_ASSIGN_OR_RETURN(
          element, iterator->Next1(descriptor_pool, message_factory, arena));
      if (!element) {
        break;
      }
      AppendToString(&joined);
      if (auto string_element = element->AsString(); string_element) {
        string_element->AppendToString(&joined);
      } else {
        ABSL_DCHECK(!element->Is<ErrorValue>());
        *result =
            ErrorValue(runtime_internal::CreateNoMatchingOverloadError("join"));
        return absl::OkStatus();
      }
    }
  }

  if (joined.size() > common_internal::kSmallByteStringCapacity) {
    joined.shrink_to_fit();
  }

  *result = StringValue::From(std::move(joined), arena);
  return absl::OkStatus();
}

absl::Status StringValue::Split(const StringValue& delimiter, int64_t limit,
                                google::protobuf::Arena* absl_nonnull arena,
                                Value* absl_nonnull result) const {
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  if (limit == 0) {
    // Per spec, when limit is 0 return an empty list.
    *result = ListValue();
    return absl::OkStatus();
  }
  if (limit < 0) {
    // Per spec, when limit is negative treat it as unlimited splits.
    limit = std::numeric_limits<int64_t>::max();
  }

  std::vector<std::pair<size_t, size_t>> splits;
  size_t pos = 0;
  const size_t len = value_.size();

  if (delimiter.IsEmpty()) {
    value_.Visit(absl::Overload(
        [&](absl::string_view s) {
          while (pos < len && limit > 1) {
            size_t char_len = cel::internal::Utf8Decode(s.substr(pos), nullptr);
            splits.push_back({pos, pos + char_len});
            pos += char_len;
            --limit;
          }
        },
        [&](const absl::Cord& s) {
          while (pos < len && limit > 1) {
            size_t char_len = cel::internal::Utf8Decode(
                s.Subcord(pos, len - pos).char_begin(), nullptr);
            splits.push_back({pos, pos + char_len});
            pos += char_len;
            --limit;
          }
        }));
  } else {
    while (pos < len && limit > 1) {
      absl::optional<size_t> next = value_.Find(delimiter.value_, pos);
      if (!next) {
        break;
      }
      splits.push_back(std::pair{pos, *next});
      pos = *next + delimiter.value_.size();
      --limit;
      ABSL_DCHECK_LE(pos, len);
    }
  }

  if (splits.empty() || !delimiter.IsEmpty() || pos < len) {
    splits.push_back(std::pair{pos, len});
  }

  auto builder = NewListValueBuilder(arena);
  builder->Reserve(splits.size());
  for (const std::pair<size_t, size_t>& split : splits) {
    builder->UnsafeAdd(
        StringValue(value_.Substring(split.first, split.second)));
  }
  *result = std::move(*builder).Build();
  return absl::OkStatus();
}

absl::Status StringValue::Replace(const StringValue& needle,
                                  const StringValue& replacement, int64_t limit,
                                  google::protobuf::Arena* absl_nonnull arena,
                                  Value* absl_nonnull result) const {
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  if (limit == 0) {
    // Per spec, when limit is 0 return the original string.
    *result = *this;
    return absl::OkStatus();
  }
  if (limit < 0) {
    // Per spec, when limit is negative treat it as unlimited replacements.
    limit = std::numeric_limits<int64_t>::max();
  }

  size_t pos = 0;
  const size_t len = value_.size();
  const size_t needle_len = needle.value_.size();
  std::string res_str;

  if (needle.IsEmpty()) {
    value_.Visit(absl::Overload(
        [&](absl::string_view s) {
          while (pos < len && limit > 0) {
            replacement.AppendToString(&res_str);
            size_t char_len = cel::internal::Utf8Decode(s.substr(pos), nullptr);
            value_.Substring(pos, pos + char_len).AppendToString(&res_str);
            pos += char_len;
            --limit;
          }
        },
        [&](const absl::Cord& s) {
          while (pos < len && limit > 0) {
            replacement.AppendToString(&res_str);
            size_t char_len = cel::internal::Utf8Decode(
                s.Subcord(pos, len - pos).char_begin(), nullptr);
            value_.Substring(pos, pos + char_len).AppendToString(&res_str);
            pos += char_len;
            --limit;
          }
        }));
    if (limit > 0) {
      replacement.AppendToString(&res_str);
    }
  } else {
    while (pos < len && limit > 0) {
      absl::optional<size_t> next = value_.Find(needle.value_, pos);
      if (!next) {
        break;
      }

      value_.Substring(pos, *next).AppendToString(&res_str);
      replacement.AppendToString(&res_str);

      pos = *next + needle_len;
      --limit;
    }
  }

  if (pos < len) {
    value_.Substring(pos, len).AppendToString(&res_str);
  }

  if (res_str.size() > common_internal::kSmallByteStringCapacity) {
    res_str.shrink_to_fit();
  }

  *result = StringValue::From(std::move(res_str), arena);
  return absl::OkStatus();
}

absl::StatusOr<Value> StringValue::Join(
    const ListValue& list,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

  Value result;
  CEL_RETURN_IF_ERROR(
      Join(list, descriptor_pool, message_factory, arena, &result));
  return result;
}

absl::StatusOr<Value> StringValue::Split(
    const StringValue& delimiter, int64_t limit,
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(arena != nullptr);

  Value result;
  CEL_RETURN_IF_ERROR(Split(delimiter, limit, arena, &result));
  return result;
}

absl::Status StringValue::Split(const StringValue& delimiter,
                                google::protobuf::Arena* absl_nonnull arena,
                                Value* absl_nonnull result) const {
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  return Split(delimiter, /*limit=*/-1, arena, result);
}

absl::StatusOr<Value> StringValue::Split(
    const StringValue& delimiter, google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(arena != nullptr);

  return Split(delimiter, /*limit=*/-1, arena);
}

absl::StatusOr<Value> StringValue::Replace(
    const StringValue& needle, const StringValue& replacement, int64_t limit,
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(arena != nullptr);

  Value result;
  CEL_RETURN_IF_ERROR(Replace(needle, replacement, limit, arena, &result));
  return result;
}

absl::Status StringValue::Replace(const StringValue& needle,
                                  const StringValue& replacement,
                                  google::protobuf::Arena* absl_nonnull arena,
                                  Value* absl_nonnull result) const {
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  return Replace(needle, replacement, /*limit=*/-1, arena, result);
}

absl::StatusOr<Value> StringValue::Replace(
    const StringValue& needle, const StringValue& replacement,
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(arena != nullptr);

  return Replace(needle, replacement, /*limit=*/-1, arena);
}

}  // namespace cel
