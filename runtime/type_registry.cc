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

#include "runtime/type_registry.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "base/handle.h"
#include "base/memory.h"
#include "base/type.h"
#include "base/type_factory.h"
#include "base/type_provider.h"
#include "base/types/enum_type.h"
#include "base/types/struct_type.h"
#include "base/value.h"
#include "common/native_type.h"
#include "internal/status_macros.h"

namespace cel {

namespace {

using EnumMap = absl::flat_hash_map<std::string, cel::Handle<cel::EnumType>>;

// Type factory for ref-counted type instances.
cel::TypeFactory& GetDefaultTypeFactory() {
  static TypeFactory* factory = new TypeFactory(cel::MemoryManager::Global());
  return *factory;
}

// EnumType implementation for generic enums that are defined at runtime that
// can be resolved in expressions.
//
// Note: this implementation is primarily used for inspecting the full set of
// enum constants rather than looking up constants by name or number.
class ResolveableEnumType final : public cel::EnumType {
 public:
  using Constant = EnumType::Constant;
  using Enumerator = TypeRegistry::Enumerator;

  ResolveableEnumType(std::string name, std::vector<Enumerator> enumerators)
      : name_(std::move(name)), enumerators_(std::move(enumerators)) {}

  static const ResolveableEnumType& Cast(const Type& type) {
    ABSL_ASSERT(Is(type));
    return static_cast<const ResolveableEnumType&>(type);
  }

  absl::string_view name() const override { return name_; }

  size_t constant_count() const override { return enumerators_.size(); };

  absl::StatusOr<absl::Nonnull<std::unique_ptr<ConstantIterator>>>
  NewConstantIterator(MemoryManager& memory_manager) const override {
    return std::make_unique<Iterator>(enumerators_);
  }

  const std::vector<Enumerator>& enumerators() const { return enumerators_; }

  absl::StatusOr<absl::optional<Constant>> FindConstantByName(
      absl::string_view name) const override;

  absl::StatusOr<absl::optional<Constant>> FindConstantByNumber(
      int64_t number) const override;

 private:
  class Iterator : public EnumType::ConstantIterator {
   public:
    using Constant = EnumType::Constant;

    explicit Iterator(absl::Span<const Enumerator> enumerators)
        : idx_(0), enumerators_(enumerators) {}

    bool HasNext() override { return idx_ < enumerators_.size(); }

    absl::StatusOr<Constant> Next() override {
      if (!HasNext()) {
        return absl::FailedPreconditionError(
            "Next() called when HasNext() false in "
            "ResolveableEnumType::Iterator");
      }
      int current = idx_;
      idx_++;
      return Constant(MakeConstantId(enumerators_[current].number),
                      enumerators_[current].name, enumerators_[current].number);
    }

    absl::StatusOr<absl::string_view> NextName() override {
      CEL_ASSIGN_OR_RETURN(Constant constant, Next());

      return constant.name;
    }

    absl::StatusOr<int64_t> NextNumber() override {
      CEL_ASSIGN_OR_RETURN(Constant constant, Next());

      return constant.number;
    }

   private:
    // The index for the next returned value.
    int idx_;
    absl::Span<const Enumerator> enumerators_;
  };

  // Implement EnumType.
  cel::NativeTypeId GetNativeTypeId() const override {
    return cel::NativeTypeId::For<ResolveableEnumType>();
  }

  std::string name_;
  // TODO(uncreated-issue/42): this could be indexed by name and/or number if strong
  // enum typing is needed at runtime.
  std::vector<Enumerator> enumerators_;
};

absl::StatusOr<absl::optional<ResolveableEnumType::Constant>>
ResolveableEnumType::FindConstantByName(absl::string_view name) const {
  for (const Enumerator& enumerator : enumerators_) {
    if (enumerator.name == name) {
      return ResolveableEnumType::Constant(MakeConstantId(enumerator.number),
                                           enumerator.name, enumerator.number);
    }
  }
  return absl::nullopt;
}

absl::StatusOr<absl::optional<ResolveableEnumType::Constant>>
ResolveableEnumType::FindConstantByNumber(int64_t number) const {
  for (const Enumerator& enumerator : enumerators_) {
    if (enumerator.number == number) {
      return ResolveableEnumType::Constant(MakeConstantId(enumerator.number),
                                           enumerator.name, enumerator.number);
    }
  }
  return absl::nullopt;
}

}  // namespace

TypeRegistry::TypeRegistry() {
  RegisterEnum("google.protobuf.NullValue", {{"NULL_VALUE", 0}});
}

void TypeRegistry::RegisterEnum(absl::string_view enum_name,
                                std::vector<Enumerator> enumerators) {
  absl::StatusOr<cel::Handle<cel::EnumType>> result_or =
      GetDefaultTypeFactory().CreateEnumType<ResolveableEnumType>(
          std::string(enum_name), std::move(enumerators));
  // For this setup, the type factory should never return an error.
  result_or.IgnoreError();
  enum_types_[enum_name] = std::move(result_or).value();
}

}  // namespace cel
