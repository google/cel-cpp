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

// IWYU pragma: private, include "common/type.h"
// IWYU pragma: friend "common/type.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_OPTIONAL_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_OPTIONAL_TYPE_H_

#include <memory>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/log/absl_check.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/sized_input_view.h"
#include "common/types/opaque_type.h"

namespace cel {

class OptionalType;
class OptionalTypeView;
class Type;
class TypeView;

class OptionalType final : public OpaqueType {
 public:
  using view_alternative_type = OptionalTypeView;

  static constexpr absl::string_view kName = "optional_type";

  using OpaqueType::OpaqueType;

  explicit OptionalType(OpaqueTypeView) = delete;

  // By default, this type is `optional(dyn)`. Unless you can help it, you
  // should choose a more specific optional type.
  OptionalType();

  OptionalType(MemoryManagerRef, absl::string_view,
               const SizedInputView<TypeView>&) = delete;

  explicit OptionalType(OptionalTypeView type);

  OptionalType(MemoryManagerRef memory_manager, TypeView parameter);

  absl::string_view name() const {
    auto name = OpaqueType::name();
    ABSL_DCHECK_EQ(name, kName);
    return name;
  }

  absl::Span<const Type> parameters() const {
    auto parameters = OpaqueType::parameters();
    ABSL_DCHECK_EQ(parameters.size(), 1);
    return parameters;
  }

  TypeView parameter() const;

 private:
  // Used by SubsumptionTraits to downcast OpaqueType rvalue references.
  explicit OptionalType(OpaqueType&& type) noexcept
      : OpaqueType(std::move(type)) {}

  friend struct SubsumptionTraits<OptionalType>;
};

template <>
struct SubsumptionTraits<OptionalType> final {
  static bool IsA(const OpaqueType& type) {
    return type.name() == OptionalType::kName && type.parameters().size() == 1;
  }

  static const OptionalType& DownCast(const OpaqueType& type) {
    ABSL_DCHECK(IsA(type));
    return *reinterpret_cast<const OptionalType*>(std::addressof(type));
  }

  static OptionalType& DownCast(OpaqueType& type) {
    ABSL_DCHECK(IsA(type));
    return *reinterpret_cast<OptionalType*>(std::addressof(type));
  }

  static OptionalType DownCast(OpaqueType&& type) {
    ABSL_DCHECK(IsA(type));
    return OptionalType(std::move(type));
  }
};

class OptionalTypeView final : public OpaqueTypeView {
 public:
  using alternative_type = OptionalType;

  static constexpr absl::string_view kName = OptionalType::kName;

  using OpaqueTypeView::OpaqueTypeView;

  // By default, this type is `optional(dyn)`. Unless you can help it, you
  // should choose a more specific optional type.
  OptionalTypeView();

  // NOLINTNEXTLINE(google-explicit-constructor)
  OptionalTypeView(const OpaqueType& type) noexcept = delete;

  // NOLINTNEXTLINE(google-explicit-constructor)
  OptionalTypeView(
      const OptionalType& type ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept;

  // NOLINTNEXTLINE(google-explicit-constructor)
  OptionalTypeView& operator=(
      const OpaqueType& type ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    OpaqueTypeView::operator=(type);
    return *this;
  }

  OptionalTypeView& operator=(OpaqueType&&) = delete;

  absl::string_view name() const {
    auto name = OpaqueTypeView::name();
    ABSL_DCHECK_EQ(name, kName);
    return name;
  }

  absl::Span<const Type> parameters() const {
    auto parameters = OpaqueTypeView::parameters();
    ABSL_DCHECK_EQ(parameters.size(), 1);
    return parameters;
  }

  TypeView parameter() const;

 private:
  friend struct SubsumptionTraits<OptionalTypeView>;

  // Used by SubsumptionTraits to downcast OpaqueTypeView.
  explicit OptionalTypeView(OpaqueTypeView type) noexcept
      : OpaqueTypeView(type) {}
};

template <>
struct SubsumptionTraits<OptionalTypeView> final {
  static bool IsA(OpaqueTypeView type) {
    return type.name() == OptionalTypeView::kName &&
           type.parameters().size() == 1;
  }

  static OptionalTypeView DownCast(OpaqueTypeView type) {
    ABSL_DCHECK(IsA(type));
    return OptionalTypeView(type);
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_OPTIONAL_TYPE_H_
