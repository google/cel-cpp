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

#include "extensions/protobuf/memory_manager.h"

#include <cstddef>
#include <memory>

#include "absl/base/nullability.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "google/protobuf/arena.h"

namespace cel {

namespace extensions {

namespace {

absl::Nonnull<void*> ProtoPoolingMemoryManagerAllocate(void* arena, size_t size,
                                                       size_t align) {
  return static_cast<google::protobuf::Arena*>(arena)->AllocateAligned(size, align);
}

bool ProtoPoolingMemoryManagerDeallocate(absl::Nonnull<void*>,
                                         absl::Nonnull<void*>, size_t,
                                         size_t) noexcept {
  return false;
}

void ProtoPoolingMemoryManagerOwnCustomDestructor(
    absl::Nonnull<void*> arena, absl::Nonnull<void*> object,
    absl::Nonnull<PoolingMemoryManagerVirtualTable::CustomDestructPtr>
        destruct) {
  static_cast<google::protobuf::Arena*>(arena)->OwnCustomDestructor(object, destruct);
}

class ProtoPoolingMemoryManager final : public PoolingMemoryManager {
 public:
  ProtoPoolingMemoryManager() = default;

  absl::Nonnull<google::protobuf::Arena*> arena() { return &arena_; }

 private:
  absl::Nonnull<void*> AllocateImpl(size_t size, size_t align) override {
    return ProtoPoolingMemoryManagerAllocate(arena(), size, align);
  }

  void OwnCustomDestructor(absl::Nonnull<void*> object,
                           absl::Nonnull<CustomDestructPtr> destruct) override {
    ProtoPoolingMemoryManagerOwnCustomDestructor(arena(), object, destruct);
  }

  NativeTypeId GetNativeTypeId() const noexcept override {
    return NativeTypeId::For<ProtoPoolingMemoryManager>();
  }

  google::protobuf::Arena arena_;
};

const PoolingMemoryManagerVirtualTable kProtoMemoryManagerVirtualTable = {
    NativeTypeId::For<google::protobuf::Arena>(),
    &ProtoPoolingMemoryManagerAllocate,
    &ProtoPoolingMemoryManagerDeallocate,
    &ProtoPoolingMemoryManagerOwnCustomDestructor,
};

}  // namespace

}  // namespace extensions

template <>
struct SubsumptionTraits<extensions::ProtoPoolingMemoryManager> final {
  static bool IsA(const PoolingMemoryManager& memory_manager) {
    return NativeTypeId::Of(memory_manager) ==
           NativeTypeId::For<extensions::ProtoPoolingMemoryManager>();
  }
};

namespace extensions {

MemoryManagerRef ProtoMemoryManagerRef(google::protobuf::Arena* arena) {
  return arena != nullptr ? MemoryManagerRef::Pooling(
                                kProtoMemoryManagerVirtualTable, *arena)
                          : MemoryManagerRef::ReferenceCounting();
}

MemoryManager ProtoMemoryManager() {
  // We use an inverted hierarchy. So we allow implicit conversion of
  // `std::unique_ptr<PoolingMemoryManager>` to `MemoryManager`.
  return std::make_unique<ProtoPoolingMemoryManager>();
}

absl::Nullable<google::protobuf::Arena*> ProtoMemoryManagerArena(
    MemoryManager& memory_manager) {
  return ProtoMemoryManagerArena(MemoryManagerRef(memory_manager));
}

absl::Nullable<google::protobuf::Arena*> ProtoMemoryManagerArena(
    MemoryManagerRef memory_manager) {
  if (InstanceOf<extensions::ProtoPoolingMemoryManager>(memory_manager)) {
    return Cast<extensions::ProtoPoolingMemoryManager>(memory_manager).arena();
  }
  if (auto virtual_dispatcher =
          As<PoolingMemoryManagerVirtualDispatcher>(memory_manager);
      virtual_dispatcher &&
      virtual_dispatcher->vtable() ==
          std::addressof(kProtoMemoryManagerVirtualTable)) {
    return static_cast<google::protobuf::Arena*>(virtual_dispatcher->callee());
  }
  return nullptr;
}

}  // namespace extensions

}  // namespace cel
