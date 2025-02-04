// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUE_ENV_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUE_ENV_H_

#include "absl/base/nullability.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

class ValueEnv;
class ValueContext;
class Runtime;

enum class EqualityUniformity {
  // Equality can be between like kinds, such as between `int` and `double`.
  kHeterogeneous = 0,
  // Equality must be between the same kinds.
  kHomogeneous = 1,
};

// Determines the behavior of field access to unset wrapper types. The correct
// specification behavior is to return null. Historically CEL C++ returned the
// default unwrapped primitive value.
enum class UnsetWrapperTypeFieldAccess {
  // Null.
  kNull = 0,
  // Default unwrapped primitive value. This is not correct.
  kDefault = 1,
};

// ValueEnv encapsulates the configured environment for values that governs
// various behaviors.
//
// It is thread compatible.
class ValueEnv {
 public:
  ValueEnv(const ValueEnv&) = delete;
  ValueEnv(ValueEnv&&) = delete;
  virtual ~ValueEnv() = default;
  ValueEnv& operator=(const ValueEnv&) = delete;
  ValueEnv& operator=(ValueEnv&&) = delete;

  EqualityUniformity equality_uniformity() const {
    return equality_uniformity_;
  }

  UnsetWrapperTypeFieldAccess unset_wrapper_type_field_access() const {
    return unset_wrapper_type_field_access_;
  }

  // Returns the `google::protobuf::DescriptorPool` used by the environment.
  absl::Nonnull<const google::protobuf::DescriptorPool*> GetDescriptorPool() const {
    return descriptor_pool_;
  }

  // Returns the `google::protobuf::MessageFactory` used by the environment.
  absl::Nonnull<google::protobuf::MessageFactory*> GetMessageFactory() const {
    return message_factory_;
  }

 protected:
  ValueEnv(EqualityUniformity equality_uniformity,
           UnsetWrapperTypeFieldAccess unset_wrapper_type_field_access,
           absl::Nonnull<const google::protobuf::DescriptorPool*> const descriptor_pool,
           absl::Nonnull<google::protobuf::MessageFactory*> const message_factory)
      : equality_uniformity_(equality_uniformity),
        unset_wrapper_type_field_access_(unset_wrapper_type_field_access),
        descriptor_pool_(descriptor_pool),
        message_factory_(message_factory) {}

 private:
  friend class Runtime;
  friend class ValueContext;

  const EqualityUniformity equality_uniformity_;
  const UnsetWrapperTypeFieldAccess unset_wrapper_type_field_access_;
  absl::Nonnull<const google::protobuf::DescriptorPool*> const descriptor_pool_;
  absl::Nonnull<google::protobuf::MessageFactory*> const message_factory_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUE_ENV_H_
