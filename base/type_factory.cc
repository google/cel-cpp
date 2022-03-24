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

#include "base/type_factory.h"

#include <utility>

#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "base/handle.h"
#include "base/type.h"

namespace cel {

namespace {

using base_internal::PersistentHandleFactory;
using base_internal::TransientHandleFactory;

}  // namespace

namespace base_internal {

class ListTypeImpl final : public ListType {
 public:
  explicit ListTypeImpl(Persistent<const Type> element)
      : element_(std::move(element)) {}

  Transient<const Type> element() const override { return element_; }

 private:
  std::pair<size_t, size_t> SizeAndAlignment() const override {
    return std::make_pair(sizeof(ListTypeImpl), alignof(ListTypeImpl));
  }

  Persistent<const Type> element_;
};

}  // namespace base_internal

Persistent<const NullType> TypeFactory::GetNullType() {
  return WrapSingletonType<NullType>();
}

Persistent<const ErrorType> TypeFactory::GetErrorType() {
  return WrapSingletonType<ErrorType>();
}

Persistent<const DynType> TypeFactory::GetDynType() {
  return WrapSingletonType<DynType>();
}

Persistent<const AnyType> TypeFactory::GetAnyType() {
  return WrapSingletonType<AnyType>();
}

Persistent<const BoolType> TypeFactory::GetBoolType() {
  return WrapSingletonType<BoolType>();
}

Persistent<const IntType> TypeFactory::GetIntType() {
  return WrapSingletonType<IntType>();
}

Persistent<const UintType> TypeFactory::GetUintType() {
  return WrapSingletonType<UintType>();
}

Persistent<const DoubleType> TypeFactory::GetDoubleType() {
  return WrapSingletonType<DoubleType>();
}

Persistent<const StringType> TypeFactory::GetStringType() {
  return WrapSingletonType<StringType>();
}

Persistent<const BytesType> TypeFactory::GetBytesType() {
  return WrapSingletonType<BytesType>();
}

Persistent<const DurationType> TypeFactory::GetDurationType() {
  return WrapSingletonType<DurationType>();
}

Persistent<const TimestampType> TypeFactory::GetTimestampType() {
  return WrapSingletonType<TimestampType>();
}

absl::StatusOr<Persistent<const ListType>> TypeFactory::CreateListType(
    const Persistent<const Type>& element) {
  absl::MutexLock lock(&mutex_);
  auto existing = list_types_.find(element);
  if (existing != list_types_.end()) {
    return existing->second;
  }
  auto list_type = PersistentHandleFactory<const ListType>::Make<
      const base_internal::ListTypeImpl>(memory_manager(), element);
  if (ABSL_PREDICT_FALSE(!list_type)) {
    // TODO(issues/5): maybe have the handle factories return statuses as
    // they can add details on the size and alignment more easily and
    // consistently?
    return absl::ResourceExhaustedError("Failed to allocate memory");
  }
  list_types_.insert({element, list_type});
  return list_type;
}

}  // namespace cel
