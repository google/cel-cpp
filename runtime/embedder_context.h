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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_EMBEDDER_CONTEXT_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_EMBEDDER_CONTEXT_H_

#include <cstddef>
#include <type_traits>

#include "absl/container/inlined_vector.h"
#include "absl/log/absl_check.h"
#include "absl/types/optional.h"
#include "common/typeinfo.h"
#include "common/value.h"

namespace cel {

// EmbedderContext is used to package custom content defined by the embedder
// during CEL evaluation. The custom content is indexed by type. Value types
// are returned as absl::optional<T> where T is the value type. Pointer types
// are returned as T*.
//
// The content values must be trivially copyable and have a size <= 16 bytes.
// These are typically pointers or small value types (e.g. primitives, enums).
//
// An all zero memory value is used to represent an empty value. The caller
// must provide some way to disambiguate if that is a meaningfully distinct
// value from nullopt / nullptr.
//
// Scope is used to provide a distinction between multiple usages of CEL in the
// same binary.
class EmbedderContext {
 public:
  template <typename Scope, typename... Args>
  static EmbedderContext From(Args... args);

  // Convenience using a default scope.
  template <typename... Args>
  static EmbedderContext From(Args... args) {
    return From<EmbedderContext, Args...>(args...);
  }

  template <typename Scope, typename T>
  std::enable_if_t<!std::is_pointer_v<T>, absl::optional<T>> Get() const;

  template <typename Scope, typename T>
  std::enable_if_t<std::is_pointer_v<T>, T> Get() const;

  template <typename T>
  std::enable_if_t<!std::is_pointer_v<T>, absl::optional<T>> Get() const {
    return Get<EmbedderContext, T>();
  }

  template <typename T>
  std::enable_if_t<std::is_pointer_v<T>, T> Get() const {
    return Get<EmbedderContext, T>();
  }

 private:
  template <typename Scope, typename T, typename... Ts>
  void Set(T arg, Ts... args);

  template <typename Scope>
  void Set() {}

  absl::InlinedVector<cel::CustomValueContent, 2> values_;
  // These are included to check for bad accesses in debug mode.
  absl::InlinedVector<TypeInfo, 2> type_ids_;
  TypeInfo scope_;
};

template <typename Scope, typename Arg, typename... Args>
void EmbedderContext::Set(Arg arg, Args... args) {
  using IndexType = std::decay_t<Arg>;
  size_t index = TypeIdInSet<Scope>::template IndexFor<IndexType>();
  if (index >= values_.size()) {
    values_.resize(index + 1, cel::CustomValueContent::Zero());
    type_ids_.resize(index + 1);
  }
  values_[index] = cel::CustomValueContent::From(arg);
  type_ids_[index] = cel::TypeId<IndexType>();
  Set<Scope>(args...);
}

template <typename Scope, typename T>
std::enable_if_t<!std::is_pointer_v<T>, absl::optional<T>>
EmbedderContext::Get() const {
  ABSL_DCHECK_EQ(cel::TypeId<Scope>(), scope_)
      << "EmbedderContext::Get wrong scope";
  using IndexType = std::decay_t<T>;
  size_t index = TypeIdInSet<Scope>::template IndexFor<IndexType>();
  if (index >= values_.size()) {
    return absl::nullopt;
  }

  const auto& content = values_[index];
  if (content.IsZero()) return absl::nullopt;

  ABSL_DCHECK_EQ(type_ids_.size(), values_.size());
  ABSL_DCHECK_EQ(type_ids_[index], cel::TypeId<IndexType>())
      << "EmbedderContext::Get wrong type id";

  return content.To<T>();
}

template <typename Scope, typename T>
std::enable_if_t<std::is_pointer_v<T>, T> EmbedderContext::Get() const {
  ABSL_DCHECK_EQ(cel::TypeId<Scope>(), scope_)
      << "EmbedderContext::Get wrong scope";
  using IndexType = std::decay_t<T>;
  size_t index = TypeIdInSet<Scope>::template IndexFor<IndexType>();
  if (index >= values_.size()) {
    return nullptr;
  }

  const auto& content = values_[index];
  if (content.IsZero()) return nullptr;

  ABSL_DCHECK_EQ(type_ids_.size(), values_.size());
  ABSL_DCHECK_EQ(type_ids_[index], cel::TypeId<IndexType>())
      << "EmbedderContext::Get wrong type id";

  return content.To<T>();
}

template <typename Scope, typename... Args>
EmbedderContext EmbedderContext::From(Args... args) {
  EmbedderContext context;
  context.scope_ = TypeId<Scope>();
  context.Set<Scope>(args...);
  return context;
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_EMBEDDER_CONTEXT_H_
