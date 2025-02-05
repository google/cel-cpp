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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUE_CONTEXT_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUE_CONTEXT_H_

#include "absl/base/nullability.h"
#include "common/value_env.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

// ValueContext encapsulates contextual information necessary to create new
// instances of values.
//
// It is thread compatible.
class ValueContext : public ValueEnv {
 public:
  absl::Nonnull<google::protobuf::Arena*> GetArena() const { return arena_; }

 protected:
  ValueContext(
      EqualityUniformity equality_uniformity,
      UnsetWrapperTypeFieldAccess unset_wrapper_type_field_access,
      absl::Nonnull<const google::protobuf::DescriptorPool*> const descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> const message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena)
      : ValueEnv(equality_uniformity, unset_wrapper_type_field_access,
                 descriptor_pool, message_factory),
        arena_(arena) {}

 private:
  absl::Nonnull<google::protobuf::Arena*> const arena_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUE_CONTEXT_H_
