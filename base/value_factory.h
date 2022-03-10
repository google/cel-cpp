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

#ifndef THIRD_PARTY_CEL_CPP_BASE_VALUE_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_BASE_VALUE_FACTORY_H_

#include <cstdint>
#include <memory>
#include <string>

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "base/handle.h"
#include "base/memory_manager.h"
#include "base/value.h"

namespace cel {

class ValueFactory {
 public:
  virtual ~ValueFactory() = default;

  Persistent<const NullValue> GetNullValue() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const ErrorValue> CreateErrorValue(absl::Status status)
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const BoolValue> CreateBoolValue(bool value)
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const IntValue> CreateIntValue(int64_t value)
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const UintValue> CreateUintValue(uint64_t value)
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const DoubleValue> CreateDoubleValue(double value)
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const BytesValue> GetBytesValue() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetEmptyBytesValue();
  }

  absl::StatusOr<Persistent<const BytesValue>> CreateBytesValue(
      const char* value) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return CreateBytesValue(absl::string_view(value));
  }

  absl::StatusOr<Persistent<const BytesValue>> CreateBytesValue(
      absl::string_view value) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return CreateBytesValue(std::string(value));
  }

  absl::StatusOr<Persistent<const BytesValue>> CreateBytesValue(
      std::string value) ABSL_ATTRIBUTE_LIFETIME_BOUND;

  absl::StatusOr<Persistent<const BytesValue>> CreateBytesValue(
      absl::Cord value) ABSL_ATTRIBUTE_LIFETIME_BOUND;

  template <typename Releaser>
  absl::StatusOr<Persistent<const BytesValue>> CreateBytesValue(
      absl::string_view value,
      Releaser&& releaser) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    if (value.empty()) {
      std::forward<Releaser>(releaser)();
      return GetEmptyBytesValue();
    }
    return CreateBytesValue(base_internal::ExternalData(
        static_cast<const void*>(value.data()), value.size(),
        std::make_unique<base_internal::ExternalDataReleaser>(
            std::forward<Releaser>(releaser))));
  }

  absl::StatusOr<Persistent<const DurationValue>> CreateDurationValue(
      absl::Duration value) ABSL_ATTRIBUTE_LIFETIME_BOUND;

  absl::StatusOr<Persistent<const TimestampValue>> CreateTimestampValue(
      absl::Time value) ABSL_ATTRIBUTE_LIFETIME_BOUND;

 protected:
  // Prevent direct intantiation until more pure virtual methods are added.
  explicit ValueFactory(MemoryManager& memory_manager)
      : memory_manager_(memory_manager) {}

  MemoryManager& memory_manager() const { return memory_manager_; }

 private:
  Persistent<const BytesValue> GetEmptyBytesValue()
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  absl::StatusOr<Persistent<const BytesValue>> CreateBytesValue(
      base_internal::ExternalData value) ABSL_ATTRIBUTE_LIFETIME_BOUND;

  MemoryManager& memory_manager_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUE_FACTORY_H_
