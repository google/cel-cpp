// Copyright 2026 Google LLC
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

#include "common/container.h"

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "internal/lexis.h"

namespace cel {
namespace {

bool IsValidQualifiedName(absl::string_view name) {
  auto dot_pos = name.find('.');
  while (dot_pos != absl::string_view::npos) {
    if (!internal::LexisIsIdentifier(name.substr(0, dot_pos))) {
      return false;
    }
    name = name.substr(dot_pos + 1);
    dot_pos = name.find('.');
  }
  return internal::LexisIsIdentifier(name);
}

bool IsValidAlias(absl::string_view alias) {
  return internal::LexisIsIdentifier(alias);
}

bool IsAbbreviationImpl(absl::string_view alias, absl::string_view name) {
  auto pos = name.rfind('.');
  return pos != std::string::npos && pos > 0 && pos < name.size() - 1 &&
         alias == name.substr(pos + 1);
}

}  // namespace

bool ExpressionContainer::AliasListing::IsAbbreviation() const {
  return IsAbbreviationImpl(alias, name);
}

absl::StatusOr<ExpressionContainer> MakeExpressionContainer(
    absl::string_view name) {
  ExpressionContainer container;

  absl::Status status = container.SetContainer(name);
  if (!status.ok()) {
    return status;
  }
  return container;
}

absl::Status ExpressionContainer::SetContainer(absl::string_view name) {
  if (name.empty()) {
    container_ = "";
    return absl::OkStatus();
  }

  if (!IsValidQualifiedName(name)) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid qualified name: ", name));
  }

  for (const auto& entry : aliases_) {
    const std::string& alias = entry.first;
    if (name == alias ||
        (name.size() > alias.size() &&
         absl::string_view(name).substr(0, alias.size()) == alias &&
         name.at(alias.size()) == '.')) {
      return absl::InvalidArgumentError(
          absl::StrCat("container name collides with alias: ", alias));
    }
  }

  container_ = std::string(name);
  return absl::OkStatus();
}

absl::Status ExpressionContainer::AddAbbreviation(absl::string_view abrev) {
  if (!IsValidQualifiedName(abrev)) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid qualified name: ", abrev));
  }

  auto pos = abrev.rfind('.');
  if (pos == 0 || pos == absl::string_view::npos || pos == abrev.size() - 1) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid qualified name: ", abrev,
                     ", wanted name of the form 'qualified.name'"));
  }

  absl::string_view alias = abrev.substr(pos + 1);
  return AddAlias(alias, abrev);
}

absl::Status ExpressionContainer::AddAlias(absl::string_view alias,
                                           absl::string_view name) {
  if (!IsValidAlias(alias)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "alias must be non-empty and simple (not qualified): ", alias));
  }

  if (!IsValidQualifiedName(name)) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid qualified name: ", name));
  }

  if (auto it = aliases_.find(alias); it != aliases_.end()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "alias collides with existing reference: ", alias, " -> ", it->second));
  }

  if (container_ == alias ||
      (container_.size() > alias.size() &&
       absl::string_view(container_).substr(0, alias.size()) == alias &&
       container_.at(alias.size()) == '.')) {
    return absl::InvalidArgumentError(
        absl::StrCat("alias collides with container name: ", alias));
  }

  aliases_.insert({std::string(alias), std::string(name)});
  return absl::OkStatus();
}

absl::string_view ExpressionContainer::FindAlias(
    absl::string_view alias) const {
  auto it = aliases_.find(alias);
  if (it != aliases_.end()) {
    return it->second;
  }
  return "";
}

std::vector<std::string> ExpressionContainer::ListAbbreviations() const {
  std::vector<std::string> res;
  for (const auto& entry : aliases_) {
    if (IsAbbreviationImpl(entry.first, entry.second)) {
      res.push_back(entry.second);
    }
  }
  return res;
}

std::vector<ExpressionContainer::AliasListing>
ExpressionContainer::ListAliases() const {
  std::vector<AliasListing> res;
  for (const auto& entry : aliases_) {
    res.push_back({entry.first, entry.second});
  }
  return res;
}

}  // namespace cel
