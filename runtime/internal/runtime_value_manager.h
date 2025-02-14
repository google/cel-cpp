// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_RUNTIME_VALUE_MANAGER_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_RUNTIME_VALUE_MANAGER_H_

#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "common/memory.h"
#include "common/type_introspector.h"
#include "common/type_reflector.h"
#include "common/value_manager.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::runtime_internal {

class RuntimeValueManager final : public ValueManager {
 public:
  RuntimeValueManager(
      absl::Nonnull<google::protobuf::Arena*> arena,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      const TypeReflector& type_reflector)
      : arena_(arena),
        descriptor_pool_(descriptor_pool),
        message_factory_(message_factory),
        type_reflector_(type_reflector) {
    ABSL_DCHECK_EQ(descriptor_pool_, type_reflector_.descriptor_pool());
  }

  MemoryManagerRef GetMemoryManager() const override {
    return MemoryManagerRef::Pooling(arena_);
  }

  absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool()
      const override {
    return descriptor_pool_;
  }

  absl::Nonnull<google::protobuf::MessageFactory*> message_factory() const override {
    return message_factory_;
  }

 protected:
  const TypeIntrospector& GetTypeIntrospector() const override {
    return type_reflector_;
  }

  const TypeReflector& GetTypeReflector() const override {
    return type_reflector_;
  }

 private:
  absl::Nonnull<google::protobuf::Arena*> const arena_;
  absl::Nonnull<const google::protobuf::DescriptorPool*> const descriptor_pool_;
  absl::Nonnull<google::protobuf::MessageFactory*> const message_factory_;
  const TypeReflector& type_reflector_;
};

}  // namespace cel::runtime_internal

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_RUNTIME_VALUE_MANAGER_H_
