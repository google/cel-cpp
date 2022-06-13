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

#ifndef THIRD_PARTY_CEL_CPP_BASE_TYPES_DYN_TYPE_H_
#define THIRD_PARTY_CEL_CPP_BASE_TYPES_DYN_TYPE_H_

#include <cstddef>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "base/kind.h"
#include "base/type.h"

namespace cel {

class DynType final : public Type {
 public:
  Kind kind() const override { return Kind::kDyn; }

  absl::string_view name() const override { return "dyn"; }

 private:
  friend class TypeFactory;
  template <typename T>
  friend class internal::NoDestructor;
  friend class base_internal::TypeHandleBase;

  // Called by base_internal::TypeHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Type& type) { return type.kind() == Kind::kDyn; }

  ABSL_ATTRIBUTE_PURE_FUNCTION static const DynType& Get();

  DynType() = default;

  DynType(const DynType&) = delete;
  DynType(DynType&&) = delete;
};

CEL_INTERNAL_TYPE_DECL(DynType);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPES_DYN_TYPE_H_
