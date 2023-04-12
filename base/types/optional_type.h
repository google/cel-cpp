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

#ifndef THIRD_PARTY_CEL_CPP_BASE_TYPES_OPTIONAL_TYPE_H_
#define THIRD_PARTY_CEL_CPP_BASE_TYPES_OPTIONAL_TYPE_H_

#include <string>
#include <utility>

#include "absl/log/absl_check.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/type.h"
#include "base/types/opaque_type.h"
#include "internal/rtti.h"

namespace cel {

class OptionalType final : public OpaqueType {
 public:
  static bool Is(const Type& type) {
    return OpaqueType::Is(type) &&
           OpaqueType::TypeId(static_cast<const OpaqueType&>(type)) ==
               internal::TypeId<OptionalType>();
  }

  using OpaqueType::Is;

  static const OptionalType& Cast(const Type& type) {
    ABSL_DCHECK(Is(type)) << "cannot cast " << type.name() << " to optional";
    return static_cast<const OptionalType&>(type);
  }

  absl::string_view name() const override { return "optional"; }

  std::string DebugString() const override;

  absl::Span<const Handle<Type>> parameters() const override {
    return absl::MakeConstSpan(&type_, 1);
  }

  const Handle<Type>& type() const { return type_; }

 private:
  friend class MemoryManager;

  // Called by Arena-based memory managers to determine whether we actually need
  // our destructor called.
  static bool IsDestructorSkippable(const OptionalType& type) noexcept {
    return base_internal::Metadata::IsDestructorSkippable(*type.type());
  }

  explicit OptionalType(Handle<Type> type) : type_(std::move(type)) {}

  internal::TypeInfo TypeId() const override {
    return internal::TypeId<OptionalType>();
  }

  const Handle<Type> type_;
};

extern template class Handle<OptionalType>;

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPES_OPTIONAL_TYPE_H_
