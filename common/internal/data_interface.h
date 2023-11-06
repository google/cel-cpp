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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_DATA_INTERFACE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_DATA_INTERFACE_H_

#include "absl/base/attributes.h"
#include "common/native_type.h"

namespace cel {

class TypeInterface;
class ValueInterface;

namespace common_internal {

struct ListTypeData;
struct MapTypeData;
class DataInterface;

// `DataInterface` is the abstract base class of `cel::ValueInterface` and
// `cel::TypeInterface`.
class DataInterface {
 public:
  // Static member used by `cel::InstanceOf` to determine if the instance has
  // `cel::common_internal::DataInterface` as a class.
  static bool ClassOf(const DataInterface&) { return true; }

  DataInterface(const DataInterface&) = delete;
  DataInterface(DataInterface&&) = delete;

  virtual ~DataInterface() = default;

  DataInterface& operator=(const DataInterface&) = delete;
  DataInterface& operator=(DataInterface&&) = delete;

  // ADL used by `cel::MemoryManager` to determine whether the destructor needs
  // to be called when using manual memory management.
  friend bool CelIsDestructorSkippable(const DataInterface& data) {
    return false;
  }

  // ADL used by `cel::NativeTypeId` to implement `cel::NativeTypeId::Of`.
  friend NativeTypeId CelNativeTypeIdOf(const DataInterface& data) {
    return data.GetNativeTypeId();
  }

 protected:
  DataInterface() = default;

 private:
  friend class cel::TypeInterface;
  friend class cel::ValueInterface;
  friend struct ListTypeData;
  friend struct MapTypeData;

  ABSL_ATTRIBUTE_PURE_FUNCTION
  virtual NativeTypeId GetNativeTypeId() const = 0;
};

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_DATA_INTERFACE_H_
