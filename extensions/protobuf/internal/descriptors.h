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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_DESCRIPTORS_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_DESCRIPTORS_H_

#include <memory>

#include "absl/functional/function_ref.h"
#include "base/memory.h"
#include "base/type_provider.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/message.h"

namespace cel::extensions::protobuf_internal {

// Interface capable of collecting `google::protobuf::FileDescriptorProto` relevant to the
// provided `google::protobuf::Descriptor` and creating a `google::protobuf::DescriptorDatabase`.
class DescriptorGatherer {
 public:
  virtual ~DescriptorGatherer() = default;

  virtual void Gather(const google::protobuf::Descriptor& descriptor) = 0;

  virtual std::unique_ptr<google::protobuf::DescriptorDatabase> Finish() = 0;
};

std::unique_ptr<DescriptorGatherer> NewDescriptorGatherer();

// Converts a `google::protobuf::Message` which is a generated message into the equivalent
// dynamic message. This is done by copying all the relevant descriptors into a
// custom descriptor database and creating a custom descriptor pool and message
// factory.
void WithCustomDescriptorPool(
    MemoryManagerRef memory_manager, const google::protobuf::Message& message,
    const google::protobuf::Descriptor& additional_descriptor,
    absl::FunctionRef<void(TypeProvider&, const google::protobuf::Message&)> invocable);
inline void WithCustomDescriptorPool(
    MemoryManagerRef memory_manager, const google::protobuf::Message& message,
    absl::FunctionRef<void(TypeProvider&, const google::protobuf::Message&)> invocable) {
  WithCustomDescriptorPool(memory_manager, message, *message.GetDescriptor(),
                           invocable);
}

}  // namespace cel::extensions::protobuf_internal

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_DESCRIPTORS_H_
