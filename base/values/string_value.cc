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

#include "absl/base/macros.h"
#include "base/types/string_type.h"
#include "internal/strings.h"
#include "internal/utf8.h"

namespace cel {

CEL_INTERNAL_VALUE_IMPL(StringValue);

namespace {

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

size_t StringValue::size() const {
  return absl::visit(StringValueSizeVisitor{}, rep());
}

bool StringValue::empty() const { return absl::visit(EmptyVisitor{}, rep()); }

bool StringValue::Equals(absl::string_view string) const {
  return absl::visit(EqualsVisitor<absl::string_view>(string), rep());
}

bool StringValue::Equals(const absl::Cord& string) const {
  return absl::visit(EqualsVisitor<absl::Cord>(string), rep());
}

bool StringValue::Equals(const StringValue& string) const {
  return absl::visit(EqualsVisitor<StringValue>(*this), string.rep());
}

int StringValue::Compare(absl::string_view string) const {
  return absl::visit(CompareVisitor<absl::string_view>(string), rep());
}

int StringValue::Compare(const absl::Cord& string) const {
  return absl::visit(CompareVisitor<absl::Cord>(string), rep());
}

int StringValue::Compare(const StringValue& string) const {
  return absl::visit(CompareVisitor<StringValue>(*this), string.rep());
}

std::string StringValue::ToString() const {
  return absl::visit(ToStringVisitor{}, rep());
}

absl::Cord StringValue::ToCord() const {
  switch (base_internal::Metadata::Locality(*this)) {
    case base_internal::DataLocality::kNull:
      return absl::Cord();
    case base_internal::DataLocality::kStoredInline:
      if (base_internal::Metadata::IsTriviallyCopyable(*this)) {
        return absl::MakeCordFromExternal(
            static_cast<const base_internal::InlinedStringViewStringValue*>(
                this)
                ->value_,
            []() {});
      } else {
        return static_cast<const base_internal::InlinedCordStringValue*>(this)
            ->value_;
      }
    case base_internal::DataLocality::kReferenceCounted:
      base_internal::Metadata::Ref(*this);
      return absl::MakeCordFromExternal(
          static_cast<const base_internal::StringStringValue*>(this)->value_,
          [this]() {
            if (base_internal::Metadata::Unref(*this)) {
              delete static_cast<const base_internal::StringStringValue*>(this);
            }
          });
    case base_internal::DataLocality::kArenaAllocated:
      return absl::Cord(
          static_cast<const base_internal::StringStringValue*>(this)->value_);
  }
}

std::string StringValue::DebugString() const {
  return absl::visit(StringValueDebugStringVisitor{}, rep());
}

bool StringValue::Equals(const Value& other) const {
  return kind() == other.kind() &&
         absl::visit(EqualsVisitor<StringValue>(*this),
                     static_cast<const StringValue&>(other).rep());
}

void StringValue::HashValue(absl::HashState state) const {
  absl::visit(
      HashValueVisitor(absl::HashState::combine(std::move(state), type())),
      rep());
}

base_internal::StringValueRep StringValue::rep() const {
  switch (base_internal::Metadata::Locality(*this)) {
    case base_internal::DataLocality::kNull:
      return base_internal::StringValueRep();
    case base_internal::DataLocality::kStoredInline:
      if (base_internal::Metadata::IsTriviallyCopyable(*this)) {
        return base_internal::StringValueRep(
            absl::in_place_type<absl::string_view>,
            static_cast<const base_internal::InlinedStringViewStringValue*>(
                this)
                ->value_);
      } else {
        return base_internal::StringValueRep(
            absl::in_place_type<std::reference_wrapper<const absl::Cord>>,
            std::cref(
                static_cast<const base_internal::InlinedCordStringValue*>(this)
                    ->value_));
      }
    case base_internal::DataLocality::kReferenceCounted:
      ABSL_FALLTHROUGH_INTENDED;
    case base_internal::DataLocality::kArenaAllocated:
      return base_internal::StringValueRep(
          absl::in_place_type<absl::string_view>,
          absl::string_view(
              static_cast<const base_internal::StringStringValue*>(this)
                  ->value_));
  }
}

namespace base_internal {

StringStringValue::StringStringValue(std::string value)
    : base_internal::HeapData(kKind), value_(std::move(value)) {
  // Ensure `Value*` and `base_internal::HeapData*` are not thunked.
  ABSL_ASSERT(
      reinterpret_cast<uintptr_t>(static_cast<Value*>(this)) ==
      reinterpret_cast<uintptr_t>(static_cast<base_internal::HeapData*>(this)));
}

}  // namespace base_internal

}  // namespace cel
