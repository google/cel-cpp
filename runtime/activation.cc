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

#include "runtime/activation.h"

#include <memory>
#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "common/function_descriptor.h"
#include "common/value.h"
#include "internal/status_macros.h"
#include "runtime/function.h"
#include "runtime/function_overload_reference.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

absl::StatusOr<bool> Activation::FindVariable(
    absl::string_view name,
    const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
    google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
    google::protobuf::Arena* ABSL_NONNULL arena, Value* ABSL_NONNULL result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(result != nullptr);

  auto iter = values_.find(name);
  if (iter == values_.end()) {
    return false;
  }

  const ValueEntry& entry = iter->second;
  if (entry.provider.has_value()) {
    return ProvideValue(name, descriptor_pool, message_factory, arena, result);
  }
  if (entry.value.has_value()) {
    *result = *entry.value;
    return true;
  }
  return false;
}

absl::StatusOr<bool> Activation::ProvideValue(
    absl::string_view name,
    const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
    google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
    google::protobuf::Arena* ABSL_NONNULL arena, Value* ABSL_NONNULL result) const {
  absl::MutexLock lock(&mutex_);
  auto iter = values_.find(name);
  ABSL_ASSERT(iter != values_.end());
  ValueEntry& entry = iter->second;
  if (entry.value.has_value()) {
    *result = *entry.value;
    return true;
  }

  CEL_ASSIGN_OR_RETURN(
      auto provided,
      (*entry.provider)(name, descriptor_pool, message_factory, arena));
  if (provided.has_value()) {
    entry.value = std::move(provided);
    *result = *entry.value;
    return true;
  }
  return false;
}

std::vector<FunctionOverloadReference> Activation::FindFunctionOverloads(
    absl::string_view name) const {
  std::vector<FunctionOverloadReference> result;
  auto iter = functions_.find(name);
  if (iter != functions_.end()) {
    const std::vector<FunctionEntry>& overloads = iter->second;
    result.reserve(overloads.size());
    for (const auto& overload : overloads) {
      result.push_back({*overload.descriptor, *overload.implementation});
    }
  }
  return result;
}

bool Activation::InsertOrAssignValue(absl::string_view name, Value value) {
  return values_
      .insert_or_assign(name, ValueEntry{std::move(value), absl::nullopt})
      .second;
}

bool Activation::InsertOrAssignValueProvider(absl::string_view name,
                                             ValueProvider provider) {
  return values_
      .insert_or_assign(name, ValueEntry{absl::nullopt, std::move(provider)})
      .second;
}

bool Activation::InsertFunction(const cel::FunctionDescriptor& descriptor,
                                std::unique_ptr<cel::Function> impl) {
  auto& overloads = functions_[descriptor.name()];
  for (auto& overload : overloads) {
    if (overload.descriptor->ShapeMatches(descriptor)) {
      return false;
    }
  }
  overloads.push_back(
      {std::make_unique<FunctionDescriptor>(descriptor), std::move(impl)});
  return true;
}

Activation::Activation(Activation&& other) {
  using std::swap;
  swap(*this, other);
}

Activation& Activation::operator=(Activation&& other) {
  using std::swap;
  Activation tmp(std::move(other));
  swap(*this, tmp);
  return *this;
}

}  // namespace cel
