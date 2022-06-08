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

#include "base/values/string_value.h"

#include <string>
#include <utility>

#include "base/types/string_type.h"
#include "internal/casts.h"
#include "internal/strings.h"
#include "internal/utf8.h"

namespace cel {

namespace {

using base_internal::PersistentHandleFactory;

struct StringValueDebugStringVisitor final {
  std::string operator()(absl::string_view value) const {
    return internal::FormatStringLiteral(value);
  }

  std::string operator()(const absl::Cord& value) const {
    return internal::FormatStringLiteral(static_cast<std::string>(value));
  }
};

struct ToStringVisitor final {
  std::string operator()(absl::string_view value) const {
    return std::string(value);
  }

  std::string operator()(const absl::Cord& value) const {
    return static_cast<std::string>(value);
  }
};

struct StringValueSizeVisitor final {
  size_t operator()(absl::string_view value) const {
    return internal::Utf8CodePointCount(value);
  }

  size_t operator()(const absl::Cord& value) const {
    return internal::Utf8CodePointCount(value);
  }
};

struct EmptyVisitor final {
  bool operator()(absl::string_view value) const { return value.empty(); }

  bool operator()(const absl::Cord& value) const { return value.empty(); }
};

bool EqualsImpl(absl::string_view lhs, absl::string_view rhs) {
  return lhs == rhs;
}

bool EqualsImpl(absl::string_view lhs, const absl::Cord& rhs) {
  return lhs == rhs;
}

bool EqualsImpl(const absl::Cord& lhs, absl::string_view rhs) {
  return lhs == rhs;
}

bool EqualsImpl(const absl::Cord& lhs, const absl::Cord& rhs) {
  return lhs == rhs;
}

int CompareImpl(absl::string_view lhs, absl::string_view rhs) {
  return lhs.compare(rhs);
}

int CompareImpl(absl::string_view lhs, const absl::Cord& rhs) {
  return -rhs.Compare(lhs);
}

int CompareImpl(const absl::Cord& lhs, absl::string_view rhs) {
  return lhs.Compare(rhs);
}

int CompareImpl(const absl::Cord& lhs, const absl::Cord& rhs) {
  return lhs.Compare(rhs);
}

template <typename T>
class EqualsVisitor final {
 public:
  explicit EqualsVisitor(const T& ref) : ref_(ref) {}

  bool operator()(absl::string_view value) const {
    return EqualsImpl(value, ref_);
  }

  bool operator()(const absl::Cord& value) const {
    return EqualsImpl(value, ref_);
  }

 private:
  const T& ref_;
};

template <>
class EqualsVisitor<StringValue> final {
 public:
  explicit EqualsVisitor(const StringValue& ref) : ref_(ref) {}

  bool operator()(absl::string_view value) const { return ref_.Equals(value); }

  bool operator()(const absl::Cord& value) const { return ref_.Equals(value); }

 private:
  const StringValue& ref_;
};

template <typename T>
class CompareVisitor final {
 public:
  explicit CompareVisitor(const T& ref) : ref_(ref) {}

  int operator()(absl::string_view value) const {
    return CompareImpl(value, ref_);
  }

  int operator()(const absl::Cord& value) const {
    return CompareImpl(value, ref_);
  }

 private:
  const T& ref_;
};

template <>
class CompareVisitor<StringValue> final {
 public:
  explicit CompareVisitor(const StringValue& ref) : ref_(ref) {}

  int operator()(const absl::Cord& value) const { return ref_.Compare(value); }

  int operator()(absl::string_view value) const { return ref_.Compare(value); }

 private:
  const StringValue& ref_;
};

class HashValueVisitor final {
 public:
  explicit HashValueVisitor(absl::HashState state) : state_(std::move(state)) {}

  void operator()(absl::string_view value) {
    absl::HashState::combine(std::move(state_), value);
  }

  void operator()(const absl::Cord& value) {
    absl::HashState::combine(std::move(state_), value);
  }

 private:
  absl::HashState state_;
};

}  // namespace

Persistent<const Type> StringValue::type() const {
  return PersistentHandleFactory<const Type>::MakeUnmanaged<const StringType>(
      StringType::Get());
}

size_t StringValue::size() const {
  // We lazily calculate the code point count in some circumstances. If the code
  // point count is 0 and the underlying rep is not empty we need to actually
  // calculate the size. It is okay if this is done by multiple threads
  // simultaneously, it is a benign race.
  size_t size = size_.load(std::memory_order_relaxed);
  if (size == 0 && !empty()) {
    size = absl::visit(StringValueSizeVisitor{}, rep());
    size_.store(size, std::memory_order_relaxed);
  }
  return size;
}

bool StringValue::empty() const { return absl::visit(EmptyVisitor{}, rep()); }

bool StringValue::Equals(absl::string_view string) const {
  return absl::visit(EqualsVisitor<absl::string_view>(string), rep());
}

bool StringValue::Equals(const absl::Cord& string) const {
  return absl::visit(EqualsVisitor<absl::Cord>(string), rep());
}

bool StringValue::Equals(const Persistent<const StringValue>& string) const {
  return absl::visit(EqualsVisitor<StringValue>(*this), string->rep());
}

int StringValue::Compare(absl::string_view string) const {
  return absl::visit(CompareVisitor<absl::string_view>(string), rep());
}

int StringValue::Compare(const absl::Cord& string) const {
  return absl::visit(CompareVisitor<absl::Cord>(string), rep());
}

int StringValue::Compare(const Persistent<const StringValue>& string) const {
  return absl::visit(CompareVisitor<StringValue>(*this), string->rep());
}

std::string StringValue::ToString() const {
  return absl::visit(ToStringVisitor{}, rep());
}

std::string StringValue::DebugString() const {
  return absl::visit(StringValueDebugStringVisitor{}, rep());
}

bool StringValue::Equals(const Value& other) const {
  return kind() == other.kind() &&
         absl::visit(EqualsVisitor<StringValue>(*this),
                     internal::down_cast<const StringValue&>(other).rep());
}

void StringValue::HashValue(absl::HashState state) const {
  absl::visit(
      HashValueVisitor(absl::HashState::combine(std::move(state), type())),
      rep());
}

namespace base_internal {

absl::Cord InlinedCordStringValue::ToCord(bool reference_counted) const {
  static_cast<void>(reference_counted);
  return value_;
}

void InlinedCordStringValue::CopyTo(Value& address) const {
  CEL_INTERNAL_VALUE_COPY_TO(InlinedCordStringValue, *this, address);
}

void InlinedCordStringValue::MoveTo(Value& address) {
  CEL_INTERNAL_VALUE_MOVE_TO(InlinedCordStringValue, *this, address);
}

typename InlinedCordStringValue::Rep InlinedCordStringValue::rep() const {
  return Rep(absl::in_place_type<std::reference_wrapper<const absl::Cord>>,
             std::cref(value_));
}

absl::Cord InlinedStringViewStringValue::ToCord(bool reference_counted) const {
  static_cast<void>(reference_counted);
  return absl::Cord(value_);
}

void InlinedStringViewStringValue::CopyTo(Value& address) const {
  CEL_INTERNAL_VALUE_COPY_TO(InlinedStringViewStringValue, *this, address);
}

void InlinedStringViewStringValue::MoveTo(Value& address) {
  CEL_INTERNAL_VALUE_MOVE_TO(InlinedStringViewStringValue, *this, address);
}

typename InlinedStringViewStringValue::Rep InlinedStringViewStringValue::rep()
    const {
  return Rep(absl::in_place_type<absl::string_view>, value_);
}

std::pair<size_t, size_t> StringStringValue::SizeAndAlignment() const {
  return std::make_pair(sizeof(StringStringValue), alignof(StringStringValue));
}

absl::Cord StringStringValue::ToCord(bool reference_counted) const {
  if (reference_counted) {
    Ref();
    return absl::MakeCordFromExternal(absl::string_view(value_),
                                      [this]() { Unref(); });
  }
  return absl::Cord(value_);
}

typename StringStringValue::Rep StringStringValue::rep() const {
  return Rep(absl::in_place_type<absl::string_view>, absl::string_view(value_));
}

std::pair<size_t, size_t> ExternalDataStringValue::SizeAndAlignment() const {
  return std::make_pair(sizeof(ExternalDataStringValue),
                        alignof(ExternalDataStringValue));
}

absl::Cord ExternalDataStringValue::ToCord(bool reference_counted) const {
  if (reference_counted) {
    Ref();
    return absl::MakeCordFromExternal(
        absl::string_view(static_cast<const char*>(value_.data), value_.size),
        [this]() { Unref(); });
  }
  return absl::Cord(
      absl::string_view(static_cast<const char*>(value_.data), value_.size));
}

typename ExternalDataStringValue::Rep ExternalDataStringValue::rep() const {
  return Rep(
      absl::in_place_type<absl::string_view>,
      absl::string_view(static_cast<const char*>(value_.data), value_.size));
}

}  // namespace base_internal

}  // namespace cel
