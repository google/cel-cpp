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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_STRUCT_TYPE_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_STRUCT_TYPE_H_

#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/log/die_if_null.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/handle.h"
#include "base/memory.h"
#include "base/type.h"
#include "base/type_manager.h"
#include "base/types/struct_type.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::extensions {

class ProtoTypeProvider;
class ProtoStructValue;
class ProtoType;
class ProtoValue;
namespace protobuf_internal {
class ParsedProtoStructValue;
}

class ProtoStructTypeFieldIterator;

class ProtoStructType final : public CEL_STRUCT_TYPE_CLASS {
 public:
  static bool Is(const Type& type) {
    return CEL_STRUCT_TYPE_CLASS::Is(type) &&
           cel::base_internal::GetStructTypeTypeId(
               static_cast<const StructType&>(type)) ==
               cel::internal::TypeId<ProtoStructType>();
  }

  using CEL_STRUCT_TYPE_CLASS::Is;

  static const ProtoStructType& Cast(const Type& type) {
    ABSL_ASSERT(Is(type));
    return static_cast<const ProtoStructType&>(type);
  }

  absl::string_view name() const override { return descriptor().full_name(); }

  size_t field_count() const override;

  absl::StatusOr<UniqueRef<FieldIterator>> NewFieldIterator(
      MemoryManager& memory_manager) const override;

  // Called by FindField.
  absl::StatusOr<absl::optional<Field>> FindFieldByName(
      TypeManager& type_manager, absl::string_view name) const override;

  // Called by FindField.
  absl::StatusOr<absl::optional<Field>> FindFieldByNumber(
      TypeManager& type_manager, int64_t number) const override;

  const google::protobuf::Descriptor& descriptor() const { return *descriptor_; }

 private:
  friend class ProtoStructTypeFieldIterator;
  friend class ProtoType;
  friend class ProtoValue;
  friend class ProtoTypeProvider;
  friend class ProtoStructValue;
  friend class protobuf_internal::ParsedProtoStructValue;
  friend class cel::MemoryManager;

  // Called by Arena-based memory managers to determine whether we actually need
  // our destructor called.
  CEL_INTERNAL_IS_DESTRUCTOR_SKIPPABLE() {
    // Our destructor is useless, we only hold pointers to protobuf-owned data.
    return true;
  }

  template <typename T>
  static std::enable_if_t<(!std::is_same_v<google::protobuf::Message, T> &&
                           std::is_base_of_v<google::protobuf::Message, T>),
                          absl::StatusOr<Handle<ProtoStructType>>>
  Resolve(TypeManager& type_manager) {
    return Resolve(type_manager, *T::descriptor());
  }

  static absl::StatusOr<Handle<ProtoStructType>> Resolve(
      TypeManager& type_manager, const google::protobuf::Descriptor& descriptor);

  ProtoStructType(const google::protobuf::Descriptor* descriptor,
                  google::protobuf::MessageFactory* factory)
      : descriptor_(ABSL_DIE_IF_NULL(descriptor)),  // Crash OK.
        factory_(ABSL_DIE_IF_NULL(factory)) {}      // Crash OK.

  // Called by CEL_IMPLEMENT_STRUCT_TYPE() and Is() to perform type checking.
  internal::TypeInfo TypeId() const override {
    return internal::TypeId<ProtoStructType>();
  }

  const google::protobuf::Descriptor* const descriptor_;
  google::protobuf::MessageFactory* const factory_;
};

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_STRUCT_TYPE_H_
