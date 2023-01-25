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

#include "base/values/list_value_builder.h"

#include <string>
#include <utility>

#include "internal/status_macros.h"

namespace cel {

namespace base_internal {

namespace {

template <typename T>
struct ListValueCppType;

template <>
struct ListValueCppType<BoolValue> {
  using type = bool;
};

template <>
struct ListValueCppType<IntValue> {
  using type = int64_t;
};

template <>
struct ListValueCppType<UintValue> {
  using type = uint64_t;
};

template <>
struct ListValueCppType<DoubleValue> {
  using type = double;
};

template <>
struct ListValueCppType<DurationValue> {
  using type = absl::Duration;
};

template <>
struct ListValueCppType<TimestampValue> {
  using type = absl::Time;
};

template <typename T>
struct ListValueDebugString final {
  std::string operator()(const Handle<Value>& value) const {
    return value->DebugString();
  }
};

template <>
struct ListValueDebugString<BoolValue> final {
  std::string operator()(bool value) const {
    return BoolValue::DebugString(value);
  }
};

template <>
struct ListValueDebugString<IntValue> final {
  std::string operator()(int64_t value) const {
    return IntValue::DebugString(value);
  }
};

template <>
struct ListValueDebugString<UintValue> final {
  std::string operator()(uint64_t value) const {
    return UintValue::DebugString(value);
  }
};

template <>
struct ListValueDebugString<DoubleValue> final {
  std::string operator()(double value) const {
    return DoubleValue::DebugString(value);
  }
};

template <>
struct ListValueDebugString<DurationValue> final {
  std::string operator()(absl::Duration value) const {
    return DurationValue::DebugString(value);
  }
};

template <>
struct ListValueDebugString<TimestampValue> final {
  std::string operator()(absl::Time value) const {
    return TimestampValue::DebugString(value);
  }
};

template <typename T>
struct ListValueGet final {
  Handle<Value> operator()(ValueFactory& value_factory,
                           const Handle<T>& value) const {
    static_cast<void>(value_factory);
    return value;
  }
};

template <>
struct ListValueGet<BoolValue> final {
  Handle<Value> operator()(ValueFactory& value_factory, bool value) const {
    return value_factory.CreateBoolValue(value);
  }
};

template <>
struct ListValueGet<IntValue> final {
  Handle<Value> operator()(ValueFactory& value_factory, int64_t value) const {
    return value_factory.CreateIntValue(value);
  }
};

template <>
struct ListValueGet<UintValue> final {
  Handle<Value> operator()(ValueFactory& value_factory, uint64_t value) const {
    return value_factory.CreateUintValue(value);
  }
};

template <>
struct ListValueGet<DoubleValue> final {
  Handle<Value> operator()(ValueFactory& value_factory, double value) const {
    return value_factory.CreateDoubleValue(value);
  }
};

template <>
struct ListValueGet<DurationValue> final {
  Handle<Value> operator()(ValueFactory& value_factory,
                           absl::Duration value) const {
    return value_factory.CreateUncheckedDurationValue(value);
  }
};

template <>
struct ListValueGet<TimestampValue> final {
  Handle<Value> operator()(ValueFactory& value_factory,
                           absl::Time value) const {
    return value_factory.CreateUncheckedTimestampValue(value);
  }
};

template <typename T>
class StaticListValue final : public AbstractListValue {
 public:
  StaticListValue(Handle<ListType> type,
                  std::vector<typename ListValueCppType<T>::type,
                              Allocator<typename ListValueCppType<T>::type>>
                      storage)
      : AbstractListValue(std::move(type)), storage_(std::move(storage)) {}

  std::string DebugString() const override {
    size_t count = size();
    std::string out;
    out.push_back('[');
    if (count != 0) {
      out.append(ListValueDebugString<T>{}(storage_[0]));
      for (size_t index = 1; index < count; index++) {
        out.append(", ");
        out.append(ListValueDebugString<T>{}(storage_[index]));
      }
    }
    out.push_back(']');
    return out;
  }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  absl::StatusOr<Handle<Value>> Get(ValueFactory& value_factory,
                                    size_t index) const override {
    return ListValueGet<T>{}(value_factory, storage_[index]);
  }

  internal::TypeInfo TypeId() const override {
    return internal::TypeId<StaticListValue<T>>();
  }

 private:
  const std::vector<typename ListValueCppType<T>::type,
                    Allocator<typename ListValueCppType<T>::type>>
      storage_;
};

}  // namespace

}  // namespace base_internal

absl::StatusOr<Handle<ListValue>> ListValueBuilder<Value>::Build() && {
  CEL_ASSIGN_OR_RETURN(auto type, value_factory().type_factory().CreateListType(
                                      std::move(type_)));
  return value_factory().CreateListValue<base_internal::DynamicListValue>(
      std::move(type), std::move(storage_));
}

absl::StatusOr<Handle<ListValue>> ListValueBuilder<BoolValue>::Build() && {
  CEL_ASSIGN_OR_RETURN(auto type,
                       value_factory().type_factory().CreateListType(
                           value_factory().type_factory().GetBoolType()));
  return value_factory()
      .CreateListValue<base_internal::StaticListValue<BoolValue>>(
          std::move(type), std::move(storage_));
}

absl::StatusOr<Handle<ListValue>> ListValueBuilder<IntValue>::Build() && {
  CEL_ASSIGN_OR_RETURN(auto type,
                       value_factory().type_factory().CreateListType(
                           value_factory().type_factory().GetIntType()));
  return value_factory()
      .CreateListValue<base_internal::StaticListValue<IntValue>>(
          std::move(type), std::move(storage_));
}

absl::StatusOr<Handle<ListValue>> ListValueBuilder<UintValue>::Build() && {
  CEL_ASSIGN_OR_RETURN(auto type,
                       value_factory().type_factory().CreateListType(
                           value_factory().type_factory().GetUintType()));
  return value_factory()
      .CreateListValue<base_internal::StaticListValue<UintValue>>(
          std::move(type), std::move(storage_));
}

absl::StatusOr<Handle<ListValue>> ListValueBuilder<DoubleValue>::Build() && {
  CEL_ASSIGN_OR_RETURN(auto type,
                       value_factory().type_factory().CreateListType(
                           value_factory().type_factory().GetDoubleType()));
  return value_factory()
      .CreateListValue<base_internal::StaticListValue<DoubleValue>>(
          std::move(type), std::move(storage_));
}

absl::StatusOr<Handle<ListValue>> ListValueBuilder<DurationValue>::Build() && {
  CEL_ASSIGN_OR_RETURN(auto type,
                       value_factory().type_factory().CreateListType(
                           value_factory().type_factory().GetDurationType()));
  return value_factory()
      .CreateListValue<base_internal::StaticListValue<DurationValue>>(
          std::move(type), std::move(storage_));
}

absl::StatusOr<Handle<ListValue>> ListValueBuilder<TimestampValue>::Build() && {
  CEL_ASSIGN_OR_RETURN(auto type,
                       value_factory().type_factory().CreateListType(
                           value_factory().type_factory().GetTimestampType()));
  return value_factory()
      .CreateListValue<base_internal::StaticListValue<TimestampValue>>(
          std::move(type), std::move(storage_));
}

std::string ListValueBuilder<Value>::DebugString() const {
  size_t count = size();
  std::string out;
  out.push_back('[');
  if (count != 0) {
    out.append(storage_[0]->DebugString());
    for (size_t index = 1; index < count; index++) {
      out.append(", ");
      out.append(storage_[index]->DebugString());
    }
  }
  out.push_back(']');
  return out;
}

std::string ListValueBuilder<BoolValue>::DebugString() const {
  size_t count = size();
  std::string out;
  out.push_back('[');
  if (count != 0) {
    out.append(BoolValue::DebugString(storage_[0]));
    for (size_t index = 1; index < count; index++) {
      out.append(", ");
      out.append(BoolValue::DebugString(storage_[index]));
    }
  }
  out.push_back(']');
  return out;
}

std::string ListValueBuilder<IntValue>::DebugString() const {
  size_t count = size();
  std::string out;
  out.push_back('[');
  if (count != 0) {
    out.append(IntValue::DebugString(storage_[0]));
    for (size_t index = 1; index < count; index++) {
      out.append(", ");
      out.append(IntValue::DebugString(storage_[index]));
    }
  }
  out.push_back(']');
  return out;
}

std::string ListValueBuilder<UintValue>::DebugString() const {
  size_t count = size();
  std::string out;
  out.push_back('[');
  if (count != 0) {
    out.append(UintValue::DebugString(storage_[0]));
    for (size_t index = 1; index < count; index++) {
      out.append(", ");
      out.append(UintValue::DebugString(storage_[index]));
    }
  }
  out.push_back(']');
  return out;
}

std::string ListValueBuilder<DoubleValue>::DebugString() const {
  size_t count = size();
  std::string out;
  out.push_back('[');
  if (count != 0) {
    out.append(DoubleValue::DebugString(storage_[0]));
    for (size_t index = 1; index < count; index++) {
      out.append(", ");
      out.append(DoubleValue::DebugString(storage_[index]));
    }
  }
  out.push_back(']');
  return out;
}

std::string ListValueBuilder<DurationValue>::DebugString() const {
  size_t count = size();
  std::string out;
  out.push_back('[');
  if (count != 0) {
    out.append(DurationValue::DebugString(storage_[0]));
    for (size_t index = 1; index < count; index++) {
      out.append(", ");
      out.append(DurationValue::DebugString(storage_[index]));
    }
  }
  out.push_back(']');
  return out;
}

std::string ListValueBuilder<TimestampValue>::DebugString() const {
  size_t count = size();
  std::string out;
  out.push_back('[');
  if (count != 0) {
    out.append(TimestampValue::DebugString(storage_[0]));
    for (size_t index = 1; index < count; index++) {
      out.append(", ");
      out.append(TimestampValue::DebugString(storage_[index]));
    }
  }
  out.push_back(']');
  return out;
}

}  // namespace cel
