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

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/hash/hash.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/numbers.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "base/handle.h"
#include "base/internal/data.h"
#include "base/kind.h"
#include "base/type.h"
#include "base/types/string_type.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "base/values/bytes_value.h"
#include "common/any.h"
#include "common/json.h"
#include "internal/serialize.h"
#include "internal/status_macros.h"
#include "internal/strings.h"
#include "internal/time.h"
#include "internal/utf8.h"

namespace cel {

CEL_INTERNAL_VALUE_IMPL(StringValue);

namespace {

using internal::SerializeStringValue;

struct StringValueDebugStringVisitor final {
  std::string operator()(absl::string_view value) const {
    return StringValue::DebugString(value);
  }

  std::string operator()(const absl::Cord& value) const {
    return StringValue::DebugString(value);
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

struct MatchesVisitor final {
  const RE2& re;

  bool operator()(const absl::Cord& value) const {
    if (auto flat = value.TryFlat(); flat.has_value()) {
      return RE2::PartialMatch(*flat, re);
    }
    return RE2::PartialMatch(static_cast<std::string>(value), re);
  }

  bool operator()(absl::string_view value) const {
    return RE2::PartialMatch(value, re);
  }
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

absl::StatusOr<Handle<Value>> StringValue::Equals(ValueFactory& value_factory,
                                                  const Value& other) const {
  return value_factory.CreateBoolValue(other.Is<StringValue>() &&
                                       Equals(other.As<StringValue>()));
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

bool StringValue::Matches(const RE2& re) const {
  return absl::visit(MatchesVisitor{re}, rep());
}

std::string StringValue::ToString() const {
  return absl::visit(ToStringVisitor{}, rep());
}

absl::Cord StringValue::ToCord() const {
  switch (base_internal::Metadata::Locality(*this)) {
    case base_internal::DataLocality::kNull:
      return absl::Cord();
    case base_internal::DataLocality::kStoredInline:
      if (base_internal::Metadata::IsTrivial(*this)) {
        return absl::MakeCordFromExternal(
            static_cast<const base_internal::InlinedStringViewStringValue*>(
                this)
                ->value_,
            []() {});
      } else {
        switch (base_internal::Metadata::GetInlineVariant<
                base_internal::InlinedStringValueVariant>(*this)) {
          case base_internal::InlinedStringValueVariant::kCord:
            return static_cast<const base_internal::InlinedCordStringValue*>(
                       this)
                ->value_;
          case base_internal::InlinedStringValueVariant::kStringView: {
            const auto* owner =
                static_cast<const base_internal::InlinedStringViewStringValue*>(
                    this)
                    ->owner_;
            base_internal::Metadata::Ref(*owner);
            return absl::MakeCordFromExternal(
                static_cast<const base_internal::InlinedStringViewStringValue*>(
                    this)
                    ->value_,
                [owner]() { base_internal::ValueMetadata::Unref(*owner); });
          }
        }
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

std::string StringValue::DebugString(absl::string_view value) {
  return internal::FormatStringLiteral(value);
}

std::string StringValue::DebugString(const absl::Cord& value) {
  return internal::FormatStringLiteral(static_cast<std::string>(value));
}

std::string StringValue::DebugString() const {
  return absl::visit(StringValueDebugStringVisitor{}, rep());
}

absl::StatusOr<Any> StringValue::ConvertToAny(ValueFactory&) const {
  static constexpr absl::string_view kTypeName = "google.protobuf.StringValue";
  absl::Cord data;
  CEL_RETURN_IF_ERROR(SerializeStringValue(ToCord(), data));
  return MakeAny(MakeTypeUrl(kTypeName), std::move(data));
}

absl::StatusOr<Json> StringValue::ConvertToJson(ValueFactory&) const {
  return ToCord();
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
      if (base_internal::Metadata::IsTrivial(*this)) {
        return base_internal::StringValueRep(
            absl::in_place_type<absl::string_view>,
            static_cast<const base_internal::InlinedStringViewStringValue*>(
                this)
                ->value_);
      } else {
        switch (base_internal::Metadata::GetInlineVariant<
                base_internal::InlinedStringValueVariant>(*this)) {
          case base_internal::InlinedStringValueVariant::kCord:
            return base_internal::StringValueRep(
                absl::in_place_type<std::reference_wrapper<const absl::Cord>>,
                std::cref(
                    static_cast<const base_internal::InlinedCordStringValue*>(
                        this)
                        ->value_));
          case base_internal::InlinedStringValueVariant::kStringView:
            return base_internal::StringValueRep(
                absl::in_place_type<absl::string_view>,
                static_cast<const base_internal::InlinedStringViewStringValue*>(
                    this)
                    ->value_);
        }
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

namespace {

struct FlattenStringValue {
  std::string& scratch;

  absl::string_view operator()(absl::string_view string) const {
    return string;
  }

  absl::string_view operator()(const absl::Cord& string) const {
    if (auto flat = string.TryFlat(); flat) {
      return *flat;
    }
    scratch = static_cast<std::string>(string);
    return absl::string_view(scratch);
  }
};

}  // namespace

absl::string_view StringValue::Flat(std::string& scratch) const {
  return absl::visit(FlattenStringValue{scratch}, rep());
}

absl::StatusOr<Handle<Value>> StringValue::ConvertToType(
    ValueFactory& value_factory, const Handle<Type>& type) const {
  switch (type->kind()) {
    case TypeKind::kString:
      return handle_from_this();
    case TypeKind::kBytes:
      return AsBytes(value_factory);
    case TypeKind::kType:
      return value_factory.CreateTypeValue(this->type());
    case TypeKind::kInt: {
      std::string scratch;
      int64_t number;
      if (!absl::SimpleAtoi(Flat(scratch), &number)) {
        break;
      }
      return value_factory.CreateIntValue(number);
    }
    case TypeKind::kUint: {
      std::string scratch;
      uint64_t number;
      if (!absl::SimpleAtoi(Flat(scratch), &number)) {
        break;
      }
      return value_factory.CreateUintValue(number);
    }
    case TypeKind::kDouble: {
      std::string scratch;
      double number;
      if (!absl::SimpleAtod(Flat(scratch), &number)) {
        break;
      }
      return value_factory.CreateDoubleValue(number);
    }
    case TypeKind::kDuration: {
      std::string scratch;
      auto status_or_duration = internal::ParseDuration(Flat(scratch));
      if (!status_or_duration.ok()) {
        return value_factory.CreateErrorValue(status_or_duration.status());
      }
      auto status_or_duration_value =
          value_factory.CreateDurationValue(*status_or_duration);
      if (!status_or_duration_value.ok()) {
        return value_factory.CreateErrorValue(
            status_or_duration_value.status());
      }
      return std::move(*status_or_duration_value);
    }
    case TypeKind::kTimestamp: {
      std::string scratch;
      auto status_or_timestamp = internal::ParseTimestamp(Flat(scratch));
      if (!status_or_timestamp.ok()) {
        return value_factory.CreateErrorValue(status_or_timestamp.status());
      }
      auto status_or_timestamp_value =
          value_factory.CreateTimestampValue(*status_or_timestamp);
      if (!status_or_timestamp_value.ok()) {
        return value_factory.CreateErrorValue(
            status_or_timestamp_value.status());
      }
      return std::move(*status_or_timestamp_value);
    }
    default:
      break;
  }
  return value_factory.CreateErrorValue(
      base_internal::TypeConversionError(*this->type(), *type));
}

absl::StatusOr<Handle<BytesValue>> StringValue::AsBytes(
    ValueFactory& value_factory) const {
  // Here be dragons.
  //
  // StringValue and BytesValue have equivalent representations underneath the
  // covers. For every StringValue implementation there is an analogous
  // BytesValue implementation, and vice versa. Figure out which one it is and
  // create the analogous, handling reference counting and etc.
  switch (base_internal::Metadata::Locality(*this)) {
    case base_internal::DataLocality::kNull:
      return value_factory.GetBytesValue();
    case base_internal::DataLocality::kStoredInline:
      switch (base_internal::Metadata::GetInlineVariant<
              base_internal::InlinedStringValueVariant>(*this)) {
        case base_internal::InlinedStringValueVariant::kCord:
          return base_internal::HandleFactory<BytesValue>::Make<
              base_internal::InlinedCordBytesValue>(
              static_cast<const base_internal::InlinedCordStringValue*>(this)
                  ->value_);
        case base_internal::InlinedStringValueVariant::kStringView:
          if (static_cast<const base_internal::InlinedStringViewStringValue*>(
                  this)
                  ->owner_ != nullptr) {
            base_internal::Metadata::Ref(
                *static_cast<
                     const base_internal::InlinedStringViewStringValue*>(this)
                     ->owner_);
          }
          return base_internal::HandleFactory<BytesValue>::Make<
              base_internal::InlinedStringViewBytesValue>(
              static_cast<const base_internal::InlinedStringViewStringValue*>(
                  this)
                  ->value_,
              static_cast<const base_internal::InlinedStringViewStringValue*>(
                  this)
                  ->owner_);
      }
    case base_internal::DataLocality::kReferenceCounted:
      base_internal::Metadata::Ref(*this);
      return base_internal::HandleFactory<BytesValue>::Make<
          base_internal::InlinedCordBytesValue>(absl::MakeCordFromExternal(
          static_cast<const base_internal::StringStringValue*>(this)->value_,
          [this]() {
            if (base_internal::Metadata::Unref(*this)) {
              delete static_cast<const base_internal::StringStringValue*>(this);
            }
          }));
    case base_internal::DataLocality::kArenaAllocated:
      return base_internal::HandleFactory<BytesValue>::Make<
          base_internal::InlinedStringViewBytesValue>(absl::string_view(
          static_cast<const base_internal::StringStringValue*>(this)->value_));
  }
}

namespace base_internal {

StringStringValue::StringStringValue(std::string value)
    : base_internal::HeapData(kKind), value_(std::move(value)) {
  // Ensure `Value*` and `base_internal::HeapData*` are not thunked.
  ABSL_ASSERT(
      reinterpret_cast<uintptr_t>(static_cast<cel::Value*>(this)) ==
      reinterpret_cast<uintptr_t>(static_cast<base_internal::HeapData*>(this)));
}

InlinedStringViewStringValue::~InlinedStringViewStringValue() {
  if (owner_ != nullptr) {
    ValueMetadata::Unref(*owner_);
  }
}

InlinedStringViewStringValue& InlinedStringViewStringValue::operator=(
    const InlinedStringViewStringValue& other) {
  if (ABSL_PREDICT_TRUE(this != &other)) {
    if (other.owner_ != nullptr) {
      Metadata::Ref(*other.owner_);
    }
    if (owner_ != nullptr) {
      ValueMetadata::Unref(*owner_);
    }
    value_ = other.value_;
    owner_ = other.owner_;
  }
  return *this;
}

InlinedStringViewStringValue& InlinedStringViewStringValue::operator=(
    InlinedStringViewStringValue&& other) {
  if (ABSL_PREDICT_TRUE(this != &other)) {
    if (owner_ != nullptr) {
      ValueMetadata::Unref(*owner_);
    }
    value_ = other.value_;
    owner_ = other.owner_;
    other.value_ = absl::string_view();
    other.owner_ = nullptr;
  }
  return *this;
}

}  // namespace base_internal

}  // namespace cel
