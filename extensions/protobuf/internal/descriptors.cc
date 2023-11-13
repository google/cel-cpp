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

#include "extensions/protobuf/internal/descriptors.h"

#include <memory>
#include <utility>

#include "google/protobuf/descriptor.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_check.h"
#include "extensions/protobuf/memory_manager.h"
#include "extensions/protobuf/type_provider.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"

namespace cel::extensions::protobuf_internal {

namespace {

class DescriptorGathererImpl final : public DescriptorGatherer {
 public:
  DescriptorGathererImpl() = default;

  void Gather(const google::protobuf::Descriptor& descriptor) override {
    GatherFile(*descriptor.file());
  }

  std::unique_ptr<google::protobuf::DescriptorDatabase> Finish() override {
    auto database = std::make_unique<google::protobuf::SimpleDescriptorDatabase>();
    for (auto& file : files_) {
      ABSL_CHECK(database->AddAndOwn(file.second.release()));  // Crash OK
    }
    visited_.clear();
    files_.clear();
    return database;
  }

 private:
  void GatherFile(const google::protobuf::FileDescriptor& descriptor) {
    if (!Visit(descriptor.name())) {
      return;
    }
    descriptor.CopyTo(&File(descriptor));
    int dependency_count = descriptor.dependency_count();
    for (int dependency_index = 0; dependency_index < dependency_count;
         ++dependency_index) {
      GatherFile(*descriptor.dependency(dependency_index));
    }
  }

  bool Visit(absl::string_view name) { return visited_.insert(name).second; }

  google::protobuf::FileDescriptorProto& File(const google::protobuf::FileDescriptor& descriptor) {
    return File(descriptor.name());
  }

  google::protobuf::FileDescriptorProto& File(absl::string_view name) {
    auto& file = files_[name];
    if (file == nullptr) {
      file = std::make_unique<google::protobuf::FileDescriptorProto>();
    }
    file->set_name(name);
    return *file;
  }

  absl::flat_hash_set<absl::string_view> visited_;
  absl::flat_hash_map<absl::string_view,
                      std::unique_ptr<google::protobuf::FileDescriptorProto>>
      files_;
};

}  // namespace

std::unique_ptr<DescriptorGatherer> NewDescriptorGatherer() {
  return std::make_unique<DescriptorGathererImpl>();
}

void WithCustomDescriptorPool(
    MemoryManagerRef memory_manager, const google::protobuf::Message& message,
    const google::protobuf::Descriptor& additional_descriptor,
    absl::FunctionRef<void(TypeProvider&, const google::protobuf::Message&)> invocable) {
  std::unique_ptr<google::protobuf::DescriptorDatabase> database;
  {
    auto gatherer = NewDescriptorGatherer();
    gatherer->Gather(*message.GetDescriptor());
    gatherer->Gather(additional_descriptor);
    database = gatherer->Finish();
  }
  google::protobuf::DescriptorPool pool(database.get());
  google::protobuf::DynamicMessageFactory message_factory(&pool);
  message_factory.SetDelegateToGeneratedFactory(false);
  const auto* descriptor =
      pool.FindMessageTypeByName(message.GetDescriptor()->full_name());
  ABSL_CHECK(descriptor != nullptr)  // Crash OK
      << "Unable to get descriptor for "
      << message.GetDescriptor()->full_name();
  const auto* prototype = message_factory.GetPrototype(descriptor);
  ABSL_CHECK(prototype != nullptr)  // Crash OK
      << "Unable to get prototype for " << descriptor->full_name();
  google::protobuf::Arena* arena = nullptr;
  if (ProtoMemoryManagerArena(memory_manager)) {
    arena = ProtoMemoryManagerArena(memory_manager);
  }
  auto* custom = prototype->New(arena);
  {
    absl::Cord serialized;
    ABSL_CHECK(message.SerializePartialToCord(&serialized));
    ABSL_CHECK(custom->ParsePartialFromCord(serialized));
  }
  ProtoTypeProvider type_provider(&pool, &message_factory);
  invocable(type_provider, *custom);
  if (arena == nullptr) {
    delete custom;
  }
}

}  // namespace cel::extensions::protobuf_internal
