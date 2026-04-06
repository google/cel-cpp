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

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/functional/overload.h"
#include "absl/log/absl_check.h"
#include "absl/strings/str_cat.h"
#include "absl/types/variant.h"

namespace cel {

namespace {

const TypeSpec& DefaultTypeSpec() {
  static absl::NoDestructor<TypeSpec> type(TypeSpecKind{UnsetTypeSpec()});
  return *type;
}

std::string FormatPrimitive(PrimitiveType t) {
  switch (t) {
    case PrimitiveType::kBool:
      return "bool";
    case PrimitiveType::kInt64:
      return "int";
    case PrimitiveType::kUint64:
      return "uint";
    case PrimitiveType::kDouble:
      return "double";
    case PrimitiveType::kString:
      return "string";
    case PrimitiveType::kBytes:
      return "bytes";
    default:
      return "*unspecified primitive*";
  }
}

std::string FormatWellKnown(WellKnownTypeSpec t) {
  switch (t) {
    case WellKnownTypeSpec::kAny:
      return "google.protobuf.Any";
    case WellKnownTypeSpec::kDuration:
      return "google.protobuf.Duration";
    case WellKnownTypeSpec::kTimestamp:
      return "google.protobuf.Timestamp";
    default:
      return "*unspecified well known*";
  }
}

using FormatIns = std::variant<const TypeSpec* absl_nonnull, std::string>;
using FormatStack = std::vector<FormatIns>;

void HandleFormatTypeSpec(const TypeSpec& t, FormatStack& stack,
                          std::string* out) {
  if (t.has_dyn()) {
    absl::StrAppend(out, "dyn");
  } else if (t.has_null()) {
    absl::StrAppend(out, "null");
  } else if (t.has_primitive()) {
    absl::StrAppend(out, FormatPrimitive(t.primitive()));
  } else if (t.has_wrapper()) {
    absl::StrAppend(out, "wrapper(", FormatPrimitive(t.wrapper()), ")");
  } else if (t.has_well_known()) {
    absl::StrAppend(out, FormatWellKnown(t.well_known()));
    return;
  } else if (t.has_abstract_type()) {
    const auto& abs_type = t.abstract_type();
    if (abs_type.parameter_types().empty()) {
      absl::StrAppend(out, abs_type.name());
      return;
    }
    absl::StrAppend(out, abs_type.name(), "(");
    stack.push_back(")");
    for (size_t i = abs_type.parameter_types().size(); i > 0; --i) {
      stack.push_back(&abs_type.parameter_types()[i - 1]);
      if (i > 1) {
        stack.push_back(", ");
      }
    }

  } else if (t.has_type()) {
    if (t.type() == TypeSpec()) {
      absl::StrAppend(out, "type");
      return;
    }
    absl::StrAppend(out, "type(");
    stack.push_back(")");
    stack.push_back(&t.type());
  } else if (t.has_message_type()) {
    absl::StrAppend(out, t.message_type().type());
  } else if (t.has_type_param()) {
    absl::StrAppend(out, t.type_param().type());
  } else if (t.has_list_type()) {
    absl::StrAppend(out, "list(");
    stack.push_back(")");
    stack.push_back(&t.list_type().elem_type());
  } else if (t.has_map_type()) {
    absl::StrAppend(out, "map(");
    stack.push_back(")");
    stack.push_back(&t.map_type().value_type());
    stack.push_back(", ");
    stack.push_back(&t.map_type().key_type());
  } else {
    absl::StrAppend(out, "*error*");
  }
}

TypeSpecKind CopyImpl(const TypeSpecKind& other) {
  return absl::visit(
      absl::Overload(
          [](const std::unique_ptr<TypeSpec>& other) -> TypeSpecKind {
            if (other == nullptr) {
              return std::make_unique<TypeSpec>();
            }
            return std::make_unique<TypeSpec>(*other);
          },
          [](const auto& other) -> TypeSpecKind {
            // Other variants define copy ctor.
            return other;
          }),
      other);
}

}  // namespace

const ExtensionSpec::Version& ExtensionSpec::Version::DefaultInstance() {
  static absl::NoDestructor<Version> instance;
  return *instance;
}

const ExtensionSpec& ExtensionSpec::DefaultInstance() {
  static absl::NoDestructor<ExtensionSpec> instance;
  return *instance;
}

ExtensionSpec::ExtensionSpec(const ExtensionSpec& other)
    : id_(other.id_),
      affected_components_(other.affected_components_),
      version_(other.version_ == nullptr
                   ? nullptr
                   : std::make_unique<Version>(*other.version_)) {}

ExtensionSpec& ExtensionSpec::operator=(const ExtensionSpec& other) {
  id_ = other.id_;
  affected_components_ = other.affected_components_;
  if (other.version_ != nullptr) {
    version_ = std::make_unique<Version>(other.version());
  } else {
    version_ = nullptr;
  }
  return *this;
}

const TypeSpec& ListTypeSpec::elem_type() const {
  if (elem_type_ != nullptr) {
    return *elem_type_;
  }
  return DefaultTypeSpec();
}

bool ListTypeSpec::operator==(const ListTypeSpec& other) const {
  return elem_type() == other.elem_type();
}

const TypeSpec& MapTypeSpec::key_type() const {
  if (key_type_ != nullptr) {
    return *key_type_;
  }
  return DefaultTypeSpec();
}

const TypeSpec& MapTypeSpec::value_type() const {
  if (value_type_ != nullptr) {
    return *value_type_;
  }
  return DefaultTypeSpec();
}

bool MapTypeSpec::operator==(const MapTypeSpec& other) const {
  return key_type() == other.key_type() && value_type() == other.value_type();
}

const TypeSpec& FunctionTypeSpec::result_type() const {
  if (result_type_ != nullptr) {
    return *result_type_;
  }
  return DefaultTypeSpec();
}

bool FunctionTypeSpec::operator==(const FunctionTypeSpec& other) const {
  return result_type() == other.result_type() && arg_types_ == other.arg_types_;
}

const TypeSpec& TypeSpec::type() const {
  auto* value = absl::get_if<std::unique_ptr<TypeSpec>>(&type_kind_);
  if (value != nullptr) {
    if (*value != nullptr) return **value;
  }
  return DefaultTypeSpec();
}

TypeSpec::TypeSpec(const TypeSpec& other)
    : type_kind_(CopyImpl(other.type_kind_)) {}

TypeSpec& TypeSpec::operator=(const TypeSpec& other) {
  type_kind_ = CopyImpl(other.type_kind_);
  return *this;
}

FunctionTypeSpec::FunctionTypeSpec(const FunctionTypeSpec& other)
    : result_type_(std::make_unique<TypeSpec>(other.result_type())),
      arg_types_(other.arg_types()) {}

FunctionTypeSpec& FunctionTypeSpec::operator=(const FunctionTypeSpec& other) {
  result_type_ = std::make_unique<TypeSpec>(other.result_type());
  arg_types_ = other.arg_types();
  return *this;
}

std::string FormatTypeSpec(const TypeSpec& t) {
  // Use a stack to avoid recursion.
  // Probably overly defensive, but fuzzers will often notice the recursion
  // and try to trigger it.
  std::string out;
  FormatStack seq;
  seq.push_back(&t);
  while (!seq.empty()) {
    FormatIns ins = std::move(seq.back());
    seq.pop_back();
    if (std::holds_alternative<std::string>(ins)) {
      absl::StrAppend(&out, std::get<std::string>(ins));
      continue;
    }
    ABSL_DCHECK(std::holds_alternative<const TypeSpec*>(ins));
    HandleFormatTypeSpec(*std::get<const TypeSpec*>(ins), seq, &out);
  }
  return out;
}

}  // namespace cel
