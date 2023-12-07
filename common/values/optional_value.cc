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

#include "absl/base/call_once.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value.h"

namespace cel {

namespace {

class EmptyOptionalValue final : public OptionalValueInterface {
 public:
  explicit EmptyOptionalValue(OptionalType type) : type_(std::move(type)) {}

  bool HasValue() const override { return false; }

  ValueView Value(cel::Value& scratch) const override {
    scratch = ErrorValue(
        absl::FailedPreconditionError("optional.none() dereference"));
    return scratch;
  }

 private:
  friend struct NativeTypeTraits<EmptyOptionalValue>;

  TypeView get_type() const override { return type_; }

  const OptionalType type_;
};

class FullOptionalValue final : public OptionalValueInterface {
 public:
  explicit FullOptionalValue(OptionalType type, cel::Value value)
      : type_(std::move(type)), value_(std::move(value)) {}

  bool HasValue() const override { return true; }

  ValueView Value(cel::Value&) const override { return value_; }

 private:
  friend struct NativeTypeTraits<FullOptionalValue>;

  TypeView get_type() const override { return type_; }

  const OptionalType type_;
  const cel::Value value_;
};

absl::once_flag empty_optional_value_once;
alignas(Shared<const EmptyOptionalValue>) char empty_optional_value[sizeof(
    Shared<const EmptyOptionalValue>)] = {0};

void InitializeEmptyOptionalValue() {
  ::new (static_cast<void*>(&empty_optional_value[0]))
      Shared<const EmptyOptionalValue>(
          MemoryManagerRef::Unmanaged().MakeShared<EmptyOptionalValue>(
              OptionalType()));
}

}  // namespace

std::string OptionalValueInterface::DebugString() const {
  if (HasValue()) {
    cel::Value scratch;
    return absl::StrCat("optional(", Value(scratch).DebugString(), ")");
  }
  return "optional.none()";
}

OptionalValue OptionalValue::None() {
  absl::call_once(empty_optional_value_once, InitializeEmptyOptionalValue);
  return OptionalValue(
      *reinterpret_cast<const Shared<const EmptyOptionalValue>*>(
          &empty_optional_value[0]));
}

OptionalValue OptionalValue::Of(MemoryManagerRef memory_manager,
                                cel::Value value) {
  return OptionalValue(memory_manager.MakeShared<FullOptionalValue>(
      OptionalType(), std::move(value)));
}

OptionalValueView OptionalValueView::None() {
  absl::call_once(empty_optional_value_once, InitializeEmptyOptionalValue);
  return OptionalValueView(
      *reinterpret_cast<const Shared<const EmptyOptionalValue>*>(
          &empty_optional_value[0]));
}

}  // namespace cel
