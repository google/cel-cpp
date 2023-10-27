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

#include "extensions/protobuf/enum_type.h"

#include <cstddef>
#include <limits>
#include <memory>
#include <utility>

#include "absl/base/macros.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "base/types/enum_type.h"
#include "internal/status_macros.h"
#include "google/protobuf/descriptor.h"

namespace cel::extensions {

class ProtoEnumTypeConstantIterator final : public EnumType::ConstantIterator {
 public:
  explicit ProtoEnumTypeConstantIterator(
      const google::protobuf::EnumDescriptor& descriptor)
      : descriptor_(descriptor) {}

  bool HasNext() override { return index_ < descriptor_.value_count(); }

  absl::StatusOr<Constant> Next() override {
    if (ABSL_PREDICT_FALSE(index_ >= descriptor_.value_count())) {
      return absl::FailedPreconditionError(
          "EnumType::ConstantIterator::Next() called when "
          "EnumType::ConstantIterator::HasNext() returns false");
    }
    const auto* value = descriptor_.value(index_++);
    return Constant(ProtoEnumType::MakeConstantId(value->number()),
                    value->name(), value->number(), value);
  }

 private:
  const google::protobuf::EnumDescriptor& descriptor_;
  int index_ = 0;
};

absl::StatusOr<Handle<ProtoEnumType>> ProtoEnumType::Resolve(
    TypeManager& type_manager, const google::protobuf::EnumDescriptor& descriptor) {
  CEL_ASSIGN_OR_RETURN(auto type,
                       type_manager.ResolveType(descriptor.full_name()));
  if (ABSL_PREDICT_FALSE(!type.has_value())) {
    return absl::NotFoundError(
        absl::StrCat("Missing protocol buffer enum type implementation for \"",
                     descriptor.full_name(), "\""));
  }
  if (ABSL_PREDICT_FALSE(!(*type)->Is<ProtoEnumType>())) {
    return absl::FailedPreconditionError(absl::StrCat(
        "Unexpected protocol buffer enum type implementation for \"",
        descriptor.full_name(), "\": ", (*type)->DebugString()));
  }
  return std::move(type).value().As<ProtoEnumType>();
}

size_t ProtoEnumType::constant_count() const {
  return descriptor().value_count();
}

absl::StatusOr<absl::optional<ProtoEnumType::Constant>>
ProtoEnumType::FindConstantByName(absl::string_view name) const {
  const auto* value_desc = descriptor().FindValueByName(name);
  if (ABSL_PREDICT_FALSE(value_desc == nullptr)) {
    return absl::nullopt;
  }
  ABSL_ASSERT(value_desc->name() == name);
  return Constant(MakeConstantId(value_desc->number()), value_desc->name(),
                  value_desc->number(), value_desc);
}

absl::StatusOr<absl::optional<ProtoEnumType::Constant>>
ProtoEnumType::FindConstantByNumber(int64_t number) const {
  if (ABSL_PREDICT_FALSE(number < std::numeric_limits<int>::min() ||
                         number > std::numeric_limits<int>::max())) {
    // Treat it as not found.
    return absl::nullopt;
  }
  const auto* value_desc =
      descriptor().FindValueByNumber(static_cast<int>(number));
  if (ABSL_PREDICT_FALSE(value_desc == nullptr)) {
    return absl::nullopt;
  }
  ABSL_ASSERT(value_desc->number() == number);
  return Constant(MakeConstantId(value_desc->number()), value_desc->name(),
                  value_desc->number(), value_desc);
}

absl::StatusOr<absl::Nonnull<std::unique_ptr<EnumType::ConstantIterator>>>
ProtoEnumType::NewConstantIterator(MemoryManager& memory_manager) const {
  return std::make_unique<ProtoEnumTypeConstantIterator>(descriptor());
}

}  // namespace cel::extensions
