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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/container/flat_hash_map.h"
#include "absl/hash/hash.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/utility/utility.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/sized_input_view.h"
#include "common/type.h"
#include "common/value.h"
#include "internal/status_macros.h"

namespace cel {

namespace {

class BasicEnumType;

class BasicEnumTypeValueIterator final : public EnumTypeValueIterator {
 public:
  explicit BasicEnumTypeValueIterator(const BasicEnumType& type)
      : type_(type) {}

  bool HasNext() override;

 private:
  absl::StatusOr<EnumTypeValueId> NextId() override;

  EnumType GetType() const override;

  const BasicEnumType& type_;
  size_t value_index_ = 0;
};

class BasicEnumType final : public EnumTypeInterface {
 public:
  BasicEnumType(std::string name, std::vector<EnumTypeValue> values,
                absl::flat_hash_map<int64_t, size_t> values_by_number,
                absl::flat_hash_map<absl::string_view, size_t> values_by_name)
      : name_(std::move(name)),
        values_(std::move(values)),
        values_by_number_(std::move(values_by_number)),
        values_by_name_(std::move(values_by_name)) {}

  absl::string_view name() const override { return name_; }

  size_t value_count() const override { return values_.size(); }

  absl::StatusOr<absl::Nonnull<EnumTypeValueIteratorPtr>> NewValueIterator()
      const override {
    return std::make_unique<BasicEnumTypeValueIterator>(*this);
  }

 private:
  friend class BasicEnumTypeValueIterator;

  bool Equals(const EnumTypeInterface&) const override { return true; }

  void HashValue(absl::HashState) const override {}

  absl::StatusOr<absl::optional<EnumTypeValueId>> FindIdByName(
      absl::string_view name) const override {
    if (auto it = values_by_name_.find(name); it != values_by_name_.end()) {
      return EnumTypeValueId(absl::in_place_type<int64_t>,
                             values_[it->second].number);
    }
    return absl::nullopt;
  }

  absl::StatusOr<absl::optional<EnumTypeValueId>> FindIdByNumber(
      int64_t number) const override {
    if (auto it = values_by_number_.find(number);
        it != values_by_number_.end()) {
      return EnumTypeValueId(absl::in_place_type<int64_t>,
                             values_[it->second].number);
    }
    return absl::nullopt;
  }

  absl::string_view GetNameForId(EnumTypeValueId id) const override {
    auto it = values_by_number_.find(id.Get<int64_t>());
    ABSL_CHECK(it != values_by_number_.end());  // Crash OK
    return values_[it->second].name;
  }

  int64_t GetNumberForId(EnumTypeValueId id) const override {
    auto it = values_by_number_.find(id.Get<int64_t>());
    ABSL_CHECK(it != values_by_number_.end());  // Crash OK
    return values_[it->second].number;
  }

  NativeTypeId GetNativeTypeId() const noexcept override {
    return NativeTypeId::For<BasicEnumType>();
  }

  const std::string name_;
  const std::vector<EnumTypeValue> values_;
  const absl::flat_hash_map<int64_t, size_t> values_by_number_;
  const absl::flat_hash_map<absl::string_view, size_t> values_by_name_;
};

bool BasicEnumTypeValueIterator::HasNext() {
  return value_index_ < type_.value_count();
}

absl::StatusOr<EnumTypeValueId> BasicEnumTypeValueIterator::NextId() {
  if (ABSL_PREDICT_FALSE(value_index_ >= type_.value_count())) {
    return absl::FailedPreconditionError(
        "EnumTypeValueIterator::Next() called after "
        "EnumTypeValueIterator::HasNext() returned false");
  }
  return EnumTypeValueId(absl::in_place_type<int64_t>,
                         type_.values_[value_index_++].number);
}

EnumType BasicEnumTypeValueIterator::GetType() const {
  return type_.shared_from_this();
}

}  // namespace

absl::StatusOr<absl::optional<EnumValue>> EnumTypeInterface::FindValueByName(
    absl::string_view name) const {
  CEL_ASSIGN_OR_RETURN(auto id, FindIdByName(name));
  if (!id.has_value()) {
    return absl::nullopt;
  }
  return EnumValue(EnumType(shared_from_this()), *id);
}

absl::StatusOr<absl::optional<EnumValue>> EnumTypeInterface::FindValueByNumber(
    int64_t number) const {
  CEL_ASSIGN_OR_RETURN(auto id, FindIdByNumber(number));
  if (!id.has_value()) {
    return absl::nullopt;
  }
  return EnumValue(EnumType(shared_from_this()), *id);
}

absl::StatusOr<EnumType> EnumType::Create(
    MemoryManagerRef memory_manager, absl::string_view name,
    SizedInputView<EnumTypeValueView> values) {
  if (ABSL_PREDICT_FALSE(name.empty())) {
    return absl::InvalidArgumentError("enum type name must not be empty");
  }
  std::vector<EnumTypeValue> copied_values;
  size_t values_count = 0;
  copied_values.resize(values.size());
  absl::flat_hash_map<int64_t, size_t> numbers;
  numbers.reserve(values.size());
  absl::flat_hash_map<absl::string_view, size_t> names;
  names.reserve(values.size());
  for (const auto& value : values) {
    if (value.number < 0) {
      if (!value.name.empty()) {
        return absl::InvalidArgumentError(
            absl::StrCat("enum type values require a number: `", name, ".",
                         value.name, "`"));
      }
      continue;
    }
    const auto value_index = values_count++;
    auto& copied_value = copied_values[value_index];
    copied_value = EnumTypeValue(value);
    if (!value.name.empty()) {
      auto names_insertion =
          names.insert({absl::string_view(copied_value.name), value_index});
      if (ABSL_PREDICT_FALSE(!names_insertion.second)) {
        return absl::InvalidArgumentError(absl::StrCat(
            "enum type `", name, "` contains values with duplicate names"));
      }
    }
    auto numbers_insertion = numbers.insert({copied_value.number, value_index});
    if (ABSL_PREDICT_FALSE(!numbers_insertion.second)) {
      return absl::InvalidArgumentError(absl::StrCat(
          "enum type `", name, "` contains values with duplicate names"));
    }
  }
  copied_values.resize(values_count);
  return EnumType(memory_manager.MakeShared<BasicEnumType>(
      std::string(name), std::move(copied_values), std::move(numbers),
      std::move(names)));
}

size_t EnumType::value_count() const { return interface_->value_count(); }

absl::StatusOr<absl::optional<EnumValue>> EnumType::FindValueByName(
    absl::string_view name) const {
  return interface_->FindValueByName(name);
}

absl::StatusOr<absl::optional<EnumValue>> EnumType::FindValueByNumber(
    int64_t number) const {
  return interface_->FindValueByNumber(number);
}

absl::StatusOr<absl::Nonnull<EnumTypeValueIteratorPtr>>
EnumType::NewValueIterator() const {
  return interface_->NewValueIterator();
}

size_t EnumTypeView::value_count() const { return interface_->value_count(); }

absl::StatusOr<absl::optional<EnumValue>> EnumTypeView::FindValueByName(
    absl::string_view name) const {
  return interface_->FindValueByName(name);
}

absl::StatusOr<absl::optional<EnumValue>> EnumTypeView::FindValueByNumber(
    int64_t number) const {
  return interface_->FindValueByNumber(number);
}

absl::StatusOr<absl::Nonnull<EnumTypeValueIteratorPtr>>
EnumTypeView::NewValueIterator() const {
  return interface_->NewValueIterator();
}

absl::StatusOr<EnumValue> EnumTypeValueIterator::Next() {
  CEL_ASSIGN_OR_RETURN(auto id, NextId());
  return EnumValue(GetType(), id);
}

}  // namespace cel
