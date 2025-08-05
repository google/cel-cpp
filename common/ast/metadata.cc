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

#include "common/ast/metadata.h"

#include <memory>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/functional/overload.h"
#include "absl/types/variant.h"

namespace cel {

namespace {

const TypeSpecifier& DefaultTypeSpecifier() {
  static absl::NoDestructor<TypeSpecifier> type(
      TypeSpecifierKind{UnsetTypeSpecifier()});
  return *type;
}

TypeSpecifierKind CopyImpl(const TypeSpecifierKind& other) {
  return absl::visit(
      absl::Overload(
          [](const std::unique_ptr<TypeSpecifier>& other) -> TypeSpecifierKind {
            if (other == nullptr) {
              return std::make_unique<TypeSpecifier>();
            }
            return std::make_unique<TypeSpecifier>(*other);
          },
          [](const auto& other) -> TypeSpecifierKind {
            // Other variants define copy ctor.
            return other;
          }),
      other);
}

}  // namespace

const ExtensionSpecifier::Version&
ExtensionSpecifier::Version::DefaultInstance() {
  static absl::NoDestructor<Version> instance;
  return *instance;
}

const ExtensionSpecifier& ExtensionSpecifier::DefaultInstance() {
  static absl::NoDestructor<ExtensionSpecifier> instance;
  return *instance;
}

ExtensionSpecifier::ExtensionSpecifier(const ExtensionSpecifier& other)
    : id_(other.id_),
      affected_components_(other.affected_components_),
      version_(std::make_unique<Version>(*other.version_)) {}

ExtensionSpecifier& ExtensionSpecifier::operator=(
    const ExtensionSpecifier& other) {
  id_ = other.id_;
  affected_components_ = other.affected_components_;
  version_ = std::make_unique<Version>(*other.version_);
  return *this;
}

const TypeSpecifier& ListTypeSpecifier::elem_type() const {
  if (elem_type_ != nullptr) {
    return *elem_type_;
  }
  return DefaultTypeSpecifier();
}

bool ListTypeSpecifier::operator==(const ListTypeSpecifier& other) const {
  return elem_type() == other.elem_type();
}

const TypeSpecifier& MapTypeSpecifier::key_type() const {
  if (key_type_ != nullptr) {
    return *key_type_;
  }
  return DefaultTypeSpecifier();
}

const TypeSpecifier& MapTypeSpecifier::value_type() const {
  if (value_type_ != nullptr) {
    return *value_type_;
  }
  return DefaultTypeSpecifier();
}

bool MapTypeSpecifier::operator==(const MapTypeSpecifier& other) const {
  return key_type() == other.key_type() && value_type() == other.value_type();
}

const TypeSpecifier& FunctionTypeSpecifier::result_type() const {
  if (result_type_ != nullptr) {
    return *result_type_;
  }
  return DefaultTypeSpecifier();
}

bool FunctionTypeSpecifier::operator==(
    const FunctionTypeSpecifier& other) const {
  return result_type() == other.result_type() && arg_types_ == other.arg_types_;
}

const TypeSpecifier& TypeSpecifier::type() const {
  auto* value = absl::get_if<std::unique_ptr<TypeSpecifier>>(&type_kind_);
  if (value != nullptr) {
    if (*value != nullptr) return **value;
  }
  return DefaultTypeSpecifier();
}

TypeSpecifier::TypeSpecifier(const TypeSpecifier& other)
    : type_kind_(CopyImpl(other.type_kind_)) {}

TypeSpecifier& TypeSpecifier::operator=(const TypeSpecifier& other) {
  type_kind_ = CopyImpl(other.type_kind_);
  return *this;
}

FunctionTypeSpecifier::FunctionTypeSpecifier(const FunctionTypeSpecifier& other)
    : result_type_(std::make_unique<TypeSpecifier>(other.result_type())),
      arg_types_(other.arg_types()) {}

FunctionTypeSpecifier& FunctionTypeSpecifier::operator=(
    const FunctionTypeSpecifier& other) {
  result_type_ = std::make_unique<TypeSpecifier>(other.result_type());
  arg_types_ = other.arg_types();
  return *this;
}

}  // namespace cel
