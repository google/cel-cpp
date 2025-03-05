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

#include <string>
#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "common/allocator.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

namespace {

class EmptyOptionalValue final : public OptionalValueInterface {
 public:
  EmptyOptionalValue() = default;

  OpaqueValue Clone(absl::Nonnull<google::protobuf::Arena*>) const override {
    return OptionalValue();
  }

  bool HasValue() const override { return false; }

  void Value(absl::Nonnull<cel::Value*> result) const override {
    *result = ErrorValue(
        absl::FailedPreconditionError("optional.none() dereference"));
  }
};

class FullOptionalValue final : public OptionalValueInterface {
 public:
  explicit FullOptionalValue(cel::Value value) : value_(std::move(value)) {}

  OpaqueValue Clone(absl::Nonnull<google::protobuf::Arena*> arena) const override {
    return MemoryManager::Pooling(arena).MakeShared<FullOptionalValue>(
        value_.Clone(arena));
  }

  bool HasValue() const override { return true; }

  void Value(absl::Nonnull<cel::Value*> result) const override {
    *result = value_;
  }

 private:
  friend struct NativeTypeTraits<FullOptionalValue>;

  const cel::Value value_;
};

}  // namespace

template <>
struct NativeTypeTraits<FullOptionalValue> {
  static bool SkipDestructor(const FullOptionalValue& value) {
    return NativeType::SkipDestructor(value.value_);
  }
};

std::string OptionalValueInterface::DebugString() const {
  if (HasValue()) {
    cel::Value value;
    Value(&value);
    return absl::StrCat("optional(", value.DebugString(), ")");
  }
  return "optional.none()";
}

OptionalValue OptionalValue::Of(cel::Value value, Allocator<> allocator) {
  ABSL_DCHECK(value.kind() != ValueKind::kError &&
              value.kind() != ValueKind::kUnknown);
  return OptionalValue(
      MemoryManagerRef(allocator).MakeShared<FullOptionalValue>(
          std::move(value)));
}

OptionalValue OptionalValue::None() {
  static const absl::NoDestructor<EmptyOptionalValue> empty;
  return OptionalValue(common_internal::MakeShared(&*empty, nullptr));
}

absl::Status OptionalValueInterface::Equal(
    const cel::Value& other,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena,
    absl::Nonnull<cel::Value*> result) const {
  if (auto other_value = other.AsOptional(); other_value.has_value()) {
    if (HasValue() != other_value->HasValue()) {
      *result = FalseValue();
      return absl::OkStatus();
    }
    if (!HasValue()) {
      *result = TrueValue();
      return absl::OkStatus();
    }
    cel::Value value;
    Value(&value);
    return value.Equal(other_value->Value(), descriptor_pool, message_factory,
                       arena, result);
  }
  *result = FalseValue();
  return absl::OkStatus();
}

void OptionalValue::Value(absl::Nonnull<cel::Value*> result) const {
  (*this)->Value(result);
}

cel::Value OptionalValue::Value() const {
  cel::Value result;
  Value(&result);
  return result;
}

}  // namespace cel
