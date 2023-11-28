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

namespace cel {

namespace {

class BasicStructType;

class BasicStructTypeFieldIterator final : public StructTypeFieldIterator {
 public:
  explicit BasicStructTypeFieldIterator(const BasicStructType& type)
      : type_(type) {}

  bool HasNext() override;

  absl::StatusOr<StructTypeFieldId> Next() override;

 private:
  const BasicStructType& type_;
  size_t field_index_ = 0;
};

class BasicStructType final : public StructTypeInterface {
 public:
  BasicStructType(std::string name, std::vector<StructTypeField> fields,
                  absl::flat_hash_map<int64_t, size_t> fields_by_number,
                  absl::flat_hash_map<absl::string_view, size_t> fields_by_name)
      : name_(std::move(name)),
        fields_(std::move(fields)),
        fields_by_number_(std::move(fields_by_number)),
        fields_by_name_(std::move(fields_by_name)) {}

  absl::string_view name() const override { return name_; }

  size_t field_count() const override { return fields_.size(); }

  absl::StatusOr<absl::optional<StructTypeFieldId>> FindFieldByName(
      absl::string_view name) const override {
    if (auto field = fields_by_name_.find(name);
        field != fields_by_name_.end()) {
      return StructTypeFieldId(absl::in_place_type<size_t>, field->second);
    }
    return absl::nullopt;
  }

  absl::StatusOr<absl::optional<StructTypeFieldId>> FindFieldByNumber(
      int64_t number) const override {
    if (auto field = fields_by_number_.find(number);
        field != fields_by_number_.end()) {
      return StructTypeFieldId(absl::in_place_type<size_t>, field->second);
    }
    return absl::nullopt;
  }

  absl::string_view GetFieldName(StructTypeFieldId id) const override {
    return fields_.at(id.Get<size_t>()).name;
  }

  int64_t GetFieldNumber(StructTypeFieldId id) const override {
    return fields_.at(id.Get<size_t>()).number;
  }

  absl::StatusOr<Type> GetFieldType(StructTypeFieldId id) const override {
    return fields_.at(id.Get<size_t>()).type;
  }

  absl::StatusOr<absl::Nonnull<StructTypeFieldIteratorPtr>> NewFieldIterator()
      const override {
    return std::make_unique<BasicStructTypeFieldIterator>(*this);
  }

 private:
  friend class BasicStructTypeFieldIterator;

  NativeTypeId GetNativeTypeId() const noexcept override {
    return NativeTypeId::For<BasicStructType>();
  }

  const std::string name_;
  const std::vector<StructTypeField> fields_;
  const absl::flat_hash_map<int64_t, size_t> fields_by_number_;
  const absl::flat_hash_map<absl::string_view, size_t> fields_by_name_;
};

bool BasicStructTypeFieldIterator::HasNext() {
  return field_index_ < type_.fields_.size();
}

absl::StatusOr<StructTypeFieldId> BasicStructTypeFieldIterator::Next() {
  if (ABSL_PREDICT_FALSE(field_index_ >= type_.fields_.size())) {
    return absl::FailedPreconditionError(
        "StructTypeFieldIterator::Next() called when "
        "StructTypeFieldIterator::HasNext() returns false");
  }
  return StructTypeFieldId(absl::in_place_type<size_t>, field_index_++);
}

}  // namespace

absl::StatusOr<StructType> StructType::Create(
    MemoryManagerRef memory_manager, absl::string_view name,
    SizedInputView<StructTypeFieldView> fields) {
  if (ABSL_PREDICT_FALSE(name.empty())) {
    return absl::InvalidArgumentError("struct type name must not be empty");
  }
  std::vector<StructTypeField> copied_fields;
  size_t field_count = 0;
  copied_fields.resize(fields.size());
  absl::flat_hash_map<int64_t, size_t> numbers;
  numbers.reserve(fields.size());
  absl::flat_hash_map<absl::string_view, size_t> names;
  names.reserve(fields.size());
  for (const auto& field : fields) {
    if (field.number < 0) {
      if (!field.name.empty()) {
        return absl::InvalidArgumentError(
            absl::StrCat("struct type fields require a number: `", name, ".",
                         field.name, "`"));
      }
      continue;
    }
    const auto field_index = field_count++;
    auto& copied_field = copied_fields[field_index];
    copied_field = StructTypeField(field);
    if (!field.name.empty()) {
      auto names_insertion =
          names.insert({absl::string_view(copied_field.name), field_index});
      if (ABSL_PREDICT_FALSE(!names_insertion.second)) {
        return absl::InvalidArgumentError(absl::StrCat(
            "struct type `", name, "` contains values with duplicate names"));
      }
    }
    auto numbers_insertion = numbers.insert({copied_field.number, field_index});
    if (ABSL_PREDICT_FALSE(!numbers_insertion.second)) {
      return absl::InvalidArgumentError(absl::StrCat(
          "struct type `", name, "` contains values with duplicate names"));
    }
  }
  copied_fields.resize(field_count);
  return StructType(memory_manager.MakeShared<BasicStructType>(
      std::string(name), std::move(copied_fields), std::move(numbers),
      std::move(names)));
}

size_t StructType::field_count() const { return interface_->field_count(); }

absl::StatusOr<absl::optional<StructTypeFieldId>> StructType::FindFieldByName(
    absl::string_view name) const {
  return interface_->FindFieldByName(name);
}

absl::StatusOr<absl::optional<StructTypeFieldId>> StructType::FindFieldByNumber(
    int64_t number) const {
  return interface_->FindFieldByNumber(number);
}

absl::string_view StructType::GetFieldName(StructTypeFieldId id) const {
  return interface_->GetFieldName(id);
}

int64_t StructType::GetFieldNumber(StructTypeFieldId id) const {
  return interface_->GetFieldNumber(id);
}

absl::StatusOr<Type> StructType::GetFieldType(StructTypeFieldId id) const {
  return interface_->GetFieldType(id);
}

absl::StatusOr<absl::Nonnull<StructTypeFieldIteratorPtr>>
StructType::NewFieldIterator() const {
  return interface_->NewFieldIterator();
}

size_t StructTypeView::field_count() const { return interface_->field_count(); }

absl::StatusOr<absl::optional<StructTypeFieldId>>
StructTypeView::FindFieldByName(absl::string_view name) const {
  return interface_->FindFieldByName(name);
}

absl::StatusOr<absl::optional<StructTypeFieldId>>
StructTypeView::FindFieldByNumber(int64_t number) const {
  return interface_->FindFieldByNumber(number);
}

absl::string_view StructTypeView::GetFieldName(StructTypeFieldId id) const {
  return interface_->GetFieldName(id);
}

int64_t StructTypeView::GetFieldNumber(StructTypeFieldId id) const {
  return interface_->GetFieldNumber(id);
}

absl::StatusOr<Type> StructTypeView::GetFieldType(StructTypeFieldId id) const {
  return interface_->GetFieldType(id);
}

absl::StatusOr<absl::Nonnull<StructTypeFieldIteratorPtr>>
StructTypeView::NewFieldIterator() const {
  return interface_->NewFieldIterator();
}

}  // namespace cel
