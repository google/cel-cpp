// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "eval/eval/test_type_registry.h"

#include <memory>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "eval/public/cel_type_registry.h"
#include "eval/public/containers/field_access.h"
#include "eval/public/structs/protobuf_descriptor_type_provider.h"
#include "internal/no_destructor.h"

namespace google::api::expr::runtime {

const CelTypeRegistry& TestTypeRegistry() {
  static CelTypeRegistry* registry = ([]() {
    auto registry = std::make_unique<CelTypeRegistry>();
    registry->RegisterTypeProvider(std::make_unique<ProtobufDescriptorProvider>(
        google::protobuf::DescriptorPool::generated_pool(),
        google::protobuf::MessageFactory::generated_factory()));
    return registry.release();
  }());

  return *registry;
}

}  // namespace google::api::expr::runtime
