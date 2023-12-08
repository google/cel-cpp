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

#include "absl/strings/str_cat.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/value.h"

namespace cel {

namespace {

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

}  // namespace

template <>
struct NativeTypeTraits<FullOptionalValue> {
  static bool SkipDestructor(const FullOptionalValue& value) {
    return NativeType::SkipDestructor(value.type_) &&
           NativeType::SkipDestructor(value.value_);
  }
};

std::string OptionalValueInterface::DebugString() const {
  if (HasValue()) {
    cel::Value scratch;
    return absl::StrCat("optional(", Value(scratch).DebugString(), ")");
  }
  return "optional.none()";
}

OptionalValue OptionalValue::Of(MemoryManagerRef memory_manager,
                                cel::Value value) {
  return OptionalValue(memory_manager.MakeShared<FullOptionalValue>(
      OptionalType(), std::move(value)));
}

}  // namespace cel
