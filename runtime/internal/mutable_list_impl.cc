//
// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "runtime/internal/mutable_list_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/value.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/value.h"

namespace cel::runtime_internal {
using ::cel::NativeTypeId;

MutableListValue::MutableListValue(
    cel::Unique<cel::ListValueBuilder> list_builder)
    : cel::OpaqueValueInterface(), list_builder_(std::move(list_builder)) {}

absl::Status MutableListValue::Append(cel::Value element) {
  return list_builder_->Add(std::move(element));
}

absl::StatusOr<cel::ListValue> MutableListValue::Build() && {
  return std::move(*list_builder_).Build();
}

std::string MutableListValue::DebugString() const {
  return kMutableListTypeName;
}

Type MutableListValue::GetTypeImpl(TypeManager& type_manager) const {
  return type_manager.CreateOpaqueType(kMutableListTypeName, {});
}

NativeTypeId MutableListValue::GetNativeTypeId() const {
  return cel::NativeTypeId::For<MutableListValue>();
}

}  // namespace cel::runtime_internal
