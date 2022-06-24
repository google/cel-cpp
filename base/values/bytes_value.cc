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

#include "base/values/bytes_value.h"

#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/strings/cord.h"
#include "base/internal/data.h"
#include "base/types/bytes_type.h"
#include "internal/strings.h"

namespace cel {

CEL_INTERNAL_VALUE_IMPL(BytesValue);

namespace {

struct BytesValueDebugStringVisitor final {
  std::string operator()(absl::string_view value) const {
    return internal::FormatBytesLiteral(value);
  }

  std::string operator()(const absl::Cord& value) const {
    return internal::FormatBytesLiteral(static_cast<std::string>(value));
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

struct BytesValueSizeVisitor final {
  size_t operator()(absl::string_view value) const { return value.size(); }

  size_t operator()(const absl::Cord& value) const { return value.size(); }
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
class EqualsVisitor<BytesValue> final {
 public:
  explicit EqualsVisitor(const BytesValue& ref) : ref_(ref) {}

  bool operator()(absl::string_view value) const { return ref_.Equals(value); }

  bool operator()(const absl::Cord& value) const { return ref_.Equals(value); }

 private:
  const BytesValue& ref_;
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
class CompareVisitor<BytesValue> final {
 public:
  explicit CompareVisitor(const BytesValue& ref) : ref_(ref) {}

  int operator()(const absl::Cord& value) const { return ref_.Compare(value); }

  int operator()(absl::string_view value) const { return ref_.Compare(value); }

 private:
  const BytesValue& ref_;
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

size_t BytesValue::size() const {
  return absl::visit(BytesValueSizeVisitor{}, rep());
}

bool BytesValue::empty() const { return absl::visit(EmptyVisitor{}, rep()); }

bool BytesValue::Equals(absl::string_view bytes) const {
  return absl::visit(EqualsVisitor<absl::string_view>(bytes), rep());
}

bool BytesValue::Equals(const absl::Cord& bytes) const {
  return absl::visit(EqualsVisitor<absl::Cord>(bytes), rep());
}

bool BytesValue::Equals(const BytesValue& bytes) const {
  return absl::visit(EqualsVisitor<BytesValue>(*this), bytes.rep());
}

int BytesValue::Compare(absl::string_view bytes) const {
  return absl::visit(CompareVisitor<absl::string_view>(bytes), rep());
}

int BytesValue::Compare(const absl::Cord& bytes) const {
  return absl::visit(CompareVisitor<absl::Cord>(bytes), rep());
}

int BytesValue::Compare(const BytesValue& bytes) const {
  return absl::visit(CompareVisitor<BytesValue>(*this), bytes.rep());
}

std::string BytesValue::ToString() const {
  return absl::visit(ToStringVisitor{}, rep());
}

absl::Cord BytesValue::ToCord() const {
  switch (base_internal::Metadata::Locality(*this)) {
    case base_internal::DataLocality::kNull:
      return absl::Cord();
    case base_internal::DataLocality::kStoredInline:
      if (base_internal::Metadata::IsTriviallyCopyable(*this)) {
        return absl::MakeCordFromExternal(
            static_cast<const base_internal::InlinedStringViewBytesValue*>(this)
                ->value_,
            []() {});
      } else {
        return static_cast<const base_internal::InlinedCordBytesValue*>(this)
            ->value_;
      }
    case base_internal::DataLocality::kReferenceCounted:
      base_internal::Metadata::Ref(*this);
      return absl::MakeCordFromExternal(
          static_cast<const base_internal::StringBytesValue*>(this)->value_,
          [this]() {
            if (base_internal::Metadata::Unref(*this)) {
              delete static_cast<const base_internal::StringBytesValue*>(this);
            }
          });
    case base_internal::DataLocality::kArenaAllocated:
      return absl::Cord(
          static_cast<const base_internal::StringBytesValue*>(this)->value_);
  }
}

std::string BytesValue::DebugString() const {
  return absl::visit(BytesValueDebugStringVisitor{}, rep());
}

bool BytesValue::Equals(const Value& other) const {
  return kind() == other.kind() &&
         absl::visit(EqualsVisitor<BytesValue>(*this),
                     static_cast<const BytesValue&>(other).rep());
}

void BytesValue::HashValue(absl::HashState state) const {
  absl::visit(
      HashValueVisitor(absl::HashState::combine(std::move(state), type())),
      rep());
}

base_internal::BytesValueRep BytesValue::rep() const {
  switch (base_internal::Metadata::Locality(*this)) {
    case base_internal::DataLocality::kNull:
      return base_internal::BytesValueRep();
    case base_internal::DataLocality::kStoredInline:
      if (base_internal::Metadata::IsTriviallyCopyable(*this)) {
        return base_internal::BytesValueRep(
            absl::in_place_type<absl::string_view>,
            static_cast<const base_internal::InlinedStringViewBytesValue*>(this)
                ->value_);
      } else {
        return base_internal::BytesValueRep(
            absl::in_place_type<std::reference_wrapper<const absl::Cord>>,
            std::cref(
                static_cast<const base_internal::InlinedCordBytesValue*>(this)
                    ->value_));
      }
    case base_internal::DataLocality::kReferenceCounted:
      ABSL_FALLTHROUGH_INTENDED;
    case base_internal::DataLocality::kArenaAllocated:
      return base_internal::BytesValueRep(
          absl::in_place_type<absl::string_view>,
          absl::string_view(
              static_cast<const base_internal::StringBytesValue*>(this)
                  ->value_));
  }
}

namespace base_internal {

StringBytesValue::StringBytesValue(std::string value)
    : base_internal::HeapData(kKind), value_(std::move(value)) {
  // Ensure `Value*` and `base_internal::HeapData*` are not thunked.
  ABSL_ASSERT(
      reinterpret_cast<uintptr_t>(static_cast<Value*>(this)) ==
      reinterpret_cast<uintptr_t>(static_cast<base_internal::HeapData*>(this)));
}

}  // namespace base_internal

}  // namespace cel
