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
#include <memory>
#include <string>

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/value.h"
#include "common/values/list_value_builder.h"
#include "common/values/values.h"
#include "eval/public/cel_value.h"
#include "internal/status_macros.h"
#include "internal/well_known_types.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

namespace {

using ::cel::well_known_types::ListValueReflection;
using ::cel::well_known_types::ValueReflection;
using ::google::api::expr::runtime::CelValue;

class EmptyListValue final : public common_internal::CompatListValue {
 public:
  static const EmptyListValue& Get() {
    static const absl::NoDestructor<EmptyListValue> empty;
    return *empty;
  }

  EmptyListValue() = default;

  std::string DebugString() const override { return "[]"; }

  bool IsEmpty() const override { return true; }

  size_t Size() const override { return 0; }

  absl::Status ConvertToJson(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Message*> json) const override {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(json != nullptr);
    ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                   google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE);

    ValueReflection value_reflection;
    CEL_RETURN_IF_ERROR(value_reflection.Initialize(json->GetDescriptor()));
    value_reflection.MutableListValue(json)->Clear();
    return absl::OkStatus();
  }

  absl::Status ConvertToJsonArray(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Message*> json) const override {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(json != nullptr);
    ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                   google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE);

    json->Clear();
    return absl::OkStatus();
  }

  CustomListValue Clone(absl::Nonnull<google::protobuf::Arena*>) const override {
    return CustomListValue();
  }

  int size() const override { return 0; }

  CelValue operator[](int index) const override {
    static const absl::NoDestructor<absl::Status> error(
        absl::InvalidArgumentError("index out of bounds"));
    return CelValue::CreateError(&*error);
  }

  using CompatListValue::Get;
  CelValue Get(google::protobuf::Arena* arena, int index) const override {
    if (arena == nullptr) {
      return (*this)[index];
    }
    return CelValue::CreateError(google::protobuf::Arena::Create<absl::Status>(
        arena, absl::InvalidArgumentError("index out of bounds")));
  }

 private:
  absl::Status GetImpl(size_t, absl::Nonnull<const google::protobuf::DescriptorPool*>,
                       absl::Nonnull<google::protobuf::MessageFactory*>,
                       absl::Nonnull<google::protobuf::Arena*>,
                       absl::Nonnull<Value*>) const override {
    // Not reachable, `Get` performs index checking.
    return absl::InternalError("unreachable");
  }
};

}  // namespace

namespace common_internal {

absl::Nonnull<const CompatListValue*> EmptyCompatListValue() {
  return &EmptyListValue::Get();
}

}  // namespace common_internal

class CustomListValueInterfaceIterator final : public ValueIterator {
 public:
  explicit CustomListValueInterfaceIterator(
      const CustomListValueInterface& interface)
      : interface_(interface), size_(interface_.Size()) {}

  bool HasNext() override { return index_ < size_; }

  absl::Status Next(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena,
      absl::Nonnull<Value*> result) override {
    if (ABSL_PREDICT_FALSE(index_ >= size_)) {
      return absl::FailedPreconditionError(
          "ValueIterator::Next() called when "
          "ValueIterator::HasNext() returns false");
    }
    return interface_.GetImpl(index_++, descriptor_pool, message_factory, arena,
                              result);
  }

 private:
  const CustomListValueInterface& interface_;
  const size_t size_;
  size_t index_ = 0;
};

absl::Status CustomListValueInterface::SerializeTo(
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<absl::Cord*> value) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(value != nullptr);

  ListValueReflection reflection;
  CEL_RETURN_IF_ERROR(reflection.Initialize(descriptor_pool));
  const google::protobuf::Message* prototype =
      message_factory->GetPrototype(reflection.GetDescriptor());
  if (prototype == nullptr) {
    return absl::UnknownError(
        absl::StrCat("failed to get message prototype: ",
                     reflection.GetDescriptor()->full_name()));
  }
  google::protobuf::Arena arena;
  google::protobuf::Message* message = prototype->New(&arena);
  CEL_RETURN_IF_ERROR(
      ConvertToJsonArray(descriptor_pool, message_factory, message));
  if (!message->SerializePartialToCord(value)) {
    return absl::UnknownError(
        "failed to serialize message: google.protobuf.ListValue");
  }
  return absl::OkStatus();
}

absl::Status CustomListValueInterface::Get(
    size_t index, absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const {
  if (ABSL_PREDICT_FALSE(index >= Size())) {
    *result = IndexOutOfBoundsError(index);
    return absl::OkStatus();
  }
  return GetImpl(index, descriptor_pool, message_factory, arena, result);
}

absl::Status CustomListValueInterface::ForEach(
    ForEachWithIndexCallback callback,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena) const {
  const size_t size = Size();
  for (size_t index = 0; index < size; ++index) {
    Value element;
    CEL_RETURN_IF_ERROR(
        GetImpl(index, descriptor_pool, message_factory, arena, &element));
    CEL_ASSIGN_OR_RETURN(auto ok, callback(index, element));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<absl::Nonnull<ValueIteratorPtr>>
CustomListValueInterface::NewIterator() const {
  return std::make_unique<CustomListValueInterfaceIterator>(*this);
}

absl::Status CustomListValueInterface::Equal(
    const Value& other,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const {
  if (auto list_value = other.As<ListValue>(); list_value.has_value()) {
    return ListValueEqual(*this, *list_value, descriptor_pool, message_factory,
                          arena, result);
  }
  *result = FalseValue();
  return absl::OkStatus();
}

absl::Status CustomListValueInterface::Contains(
    const Value& other,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const {
  Value outcome = BoolValue(false);
  Value equal;
  CEL_RETURN_IF_ERROR(ForEach(
      [&](size_t index, const Value& element) -> absl::StatusOr<bool> {
        CEL_RETURN_IF_ERROR(element.Equal(other, descriptor_pool,
                                          message_factory, arena, &equal));
        if (auto bool_result = As<BoolValue>(equal);
            bool_result.has_value() && bool_result->NativeValue()) {
          outcome = BoolValue(true);
          return false;
        }
        return true;
      },
      descriptor_pool, message_factory, arena));
  *result = outcome;
  return absl::OkStatus();
}

CustomListValue::CustomListValue()
    : CustomListValue(
          common_internal::MakeShared(&EmptyListValue::Get(), nullptr)) {}

CustomListValue CustomListValue::Clone(
    absl::Nonnull<google::protobuf::Arena*> arena) const {
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(*this);

  if (ABSL_PREDICT_FALSE(!interface_)) {
    return CustomListValue();
  }
  return interface_->Clone(arena);
}

}  // namespace cel
