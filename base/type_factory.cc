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

#include "base/handle.h"
#include "base/type.h"

namespace cel {

namespace {

using base_internal::TransientHandleFactory;

}  // namespace

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

}  // namespace cel
