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

#ifndef THIRD_PARTY_CEL_CPP_BASE_ATTRIBUTE_H_
#define THIRD_PARTY_CEL_CPP_BASE_ATTRIBUTE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "base/kind.h"

namespace google::api::expr {
class Expr;
namespace runtime {
class CelValue;
}
}  // namespace google::api::expr

namespace cel {

// AttributeQualifier represents a segment in
// attribute resolutuion path. A segment can be qualified by values of
// following types: string/int64_t/uint64_t/bool.
class AttributeQualifier final {
 private:
  struct ComparatorVisitor;

  using Variant = absl::variant<Kind, int64_t, uint64_t, std::string, bool>;

 public:
  // Factory method.
  //
  // TODO(issues/5): deprecate this and move it to a standalone method
  static AttributeQualifier Create(
      const google::api::expr::runtime::CelValue& value);

  AttributeQualifier(const AttributeQualifier&) = default;
  AttributeQualifier(AttributeQualifier&&) = default;

  AttributeQualifier& operator=(const AttributeQualifier&) = default;
  AttributeQualifier& operator=(AttributeQualifier&&) = default;

  Kind kind() const;

  // Family of Get... methods. Return values if requested type matches the
  // stored one.
  absl::optional<int64_t> GetInt64Key() const {
    return absl::holds_alternative<int64_t>(value_)
               ? absl::optional<int64_t>(absl::get<1>(value_))
               : absl::nullopt;
  }

  absl::optional<uint64_t> GetUint64Key() const {
    return absl::holds_alternative<uint64_t>(value_)
               ? absl::optional<uint64_t>(absl::get<2>(value_))
               : absl::nullopt;
  }

  absl::optional<absl::string_view> GetStringKey() const {
    return absl::holds_alternative<std::string>(value_)
               ? absl::optional<absl::string_view>(absl::get<3>(value_))
               : absl::nullopt;
  }

  absl::optional<bool> GetBoolKey() const {
    return absl::holds_alternative<bool>(value_)
               ? absl::optional<bool>(absl::get<4>(value_))
               : absl::nullopt;
  }

  bool operator==(const AttributeQualifier& other) const {
    return IsMatch(other);
  }

  bool operator<(const AttributeQualifier& other) const;

  bool IsMatch(absl::string_view other_key) const {
    absl::optional<absl::string_view> key = GetStringKey();
    return (key.has_value() && key.value() == other_key);
  }

  bool IsMatch(const google::api::expr::runtime::CelValue& value) const;

 private:
  friend class Attribute;
  friend struct ComparatorVisitor;

  AttributeQualifier() = default;

  template <typename T>
  AttributeQualifier(absl::in_place_type_t<T> in_place_type, T&& value)
      : value_(in_place_type, std::forward<T>(value)) {}

  bool IsMatch(const AttributeQualifier& other) const;

  // The previous implementation of Attribute preserved all CelValue
  // instances, regardless of whether they are supported in this context or not.
  // We represented unsupported types by using the first alternative and thus
  // preserve backwards compatibility with the result of `type()` above.
  Variant value_;
};

// Attribute represents resolved attribute path.
class Attribute final {
 public:
  explicit Attribute(std::string variable_name)
      : Attribute(std::move(variable_name), {}) {}

  Attribute(std::string variable_name,
            std::vector<AttributeQualifier> qualifier_path)
      : impl_(std::make_shared<Impl>(std::move(variable_name),
                                     std::move(qualifier_path))) {}

  // TODO(issues/5): remove this constructor as it pulls in proto deps
  Attribute(const google::api::expr::v1alpha1::Expr& variable,
            std::vector<AttributeQualifier> qualifier_path);

  absl::string_view variable_name() const { return impl_->variable_name; }

  bool has_variable_name() const { return !impl_->variable_name.empty(); }

  const std::vector<AttributeQualifier>& qualifier_path() const {
    return impl_->qualifier_path;
  }

  bool operator==(const Attribute& other) const;

  bool operator<(const Attribute& other) const;

  const absl::StatusOr<std::string> AsString() const;

 private:
  struct Impl final {
    Impl(std::string variable_name,
         std::vector<AttributeQualifier> qualifier_path)
        : variable_name(std::move(variable_name)),
          qualifier_path(std::move(qualifier_path)) {}

    std::string variable_name;
    std::vector<AttributeQualifier> qualifier_path;
  };

  std::shared_ptr<const Impl> impl_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_ATTRIBUTE_H_
