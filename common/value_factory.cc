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

#include "common/value_factory.h"

#include <cstddef>
#include <new>
#include <string>
#include <utility>

#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "common/internal/arena_string.h"
#include "common/internal/reference_count.h"
#include "common/memory.h"
#include "common/value.h"
#include "internal/status_macros.h"
#include "internal/time.h"
#include "internal/utf8.h"

namespace cel {

ListValue ValueFactory::GetZeroDynListValue() { return ListValue(); }

MapValue ValueFactory::GetZeroDynDynMapValue() { return MapValue(); }

MapValue ValueFactory::GetZeroStringDynMapValue() { return MapValue(); }

OptionalValue ValueFactory::GetZeroDynOptionalValue() {
  return OptionalValue();
}

namespace {

class ReferenceCountedString final : public common_internal::ReferenceCounted {
 public:
  static const ReferenceCountedString* New(std::string&& string) {
    return new ReferenceCountedString(std::move(string));
  }

  const char* data() const {
    return std::launder(reinterpret_cast<const std::string*>(&string_[0]))
        ->data();
  }

  size_t size() const {
    return std::launder(reinterpret_cast<const std::string*>(&string_[0]))
        ->size();
  }

 private:
  explicit ReferenceCountedString(std::string&& robbed) : ReferenceCounted() {
    ::new (static_cast<void*>(&string_[0])) std::string(std::move(robbed));
  }

  void Finalize() noexcept override {
    std::launder(reinterpret_cast<const std::string*>(&string_[0]))
        ->~basic_string();
  }

  alignas(std::string) char string_[sizeof(std::string)];
};

}  // namespace

static void StringDestructor(void* string) {
  static_cast<std::string*>(string)->~basic_string();
}

absl::StatusOr<BytesValue> ValueFactory::CreateBytesValue(std::string value) {
  auto memory_manager = GetMemoryManager();
  switch (memory_manager.memory_management()) {
    case MemoryManagement::kPooling: {
      auto* string = ::new (
          memory_manager.Allocate(sizeof(std::string), alignof(std::string)))
          std::string(std::move(value));
      memory_manager.OwnCustomDestructor(string, &StringDestructor);
      return BytesValue{common_internal::ArenaString(*string)};
    }
    case MemoryManagement::kReferenceCounting: {
      auto* refcount = ReferenceCountedString::New(std::move(value));
      auto bytes_value = BytesValue{common_internal::SharedByteString(
          refcount, absl::string_view(refcount->data(), refcount->size()))};
      common_internal::StrongUnref(*refcount);
      return bytes_value;
    }
  }
}

StringValue ValueFactory::CreateUncheckedStringValue(std::string value) {
  auto memory_manager = GetMemoryManager();
  switch (memory_manager.memory_management()) {
    case MemoryManagement::kPooling: {
      auto* string = ::new (
          memory_manager.Allocate(sizeof(std::string), alignof(std::string)))
          std::string(std::move(value));
      memory_manager.OwnCustomDestructor(string, &StringDestructor);
      return StringValue{common_internal::ArenaString(*string)};
    }
    case MemoryManagement::kReferenceCounting: {
      auto* refcount = ReferenceCountedString::New(std::move(value));
      auto string_value = StringValue{common_internal::SharedByteString(
          refcount, absl::string_view(refcount->data(), refcount->size()))};
      common_internal::StrongUnref(*refcount);
      return string_value;
    }
  }
}

absl::StatusOr<StringValue> ValueFactory::CreateStringValue(std::string value) {
  auto [count, ok] = internal::Utf8Validate(value);
  if (ABSL_PREDICT_FALSE(!ok)) {
    return absl::InvalidArgumentError(
        "Illegal byte sequence in UTF-8 encoded string");
  }
  return CreateUncheckedStringValue(std::move(value));
}

absl::StatusOr<StringValue> ValueFactory::CreateStringValue(absl::Cord value) {
  auto [count, ok] = internal::Utf8Validate(value);
  if (ABSL_PREDICT_FALSE(!ok)) {
    return absl::InvalidArgumentError(
        "Illegal byte sequence in UTF-8 encoded string");
  }
  return StringValue(std::move(value));
}

absl::StatusOr<DurationValue> ValueFactory::CreateDurationValue(
    absl::Duration value) {
  CEL_RETURN_IF_ERROR(internal::ValidateDuration(value));
  return DurationValue{value};
}

absl::StatusOr<TimestampValue> ValueFactory::CreateTimestampValue(
    absl::Time value) {
  CEL_RETURN_IF_ERROR(internal::ValidateTimestamp(value));
  return TimestampValue{value};
}

}  // namespace cel
