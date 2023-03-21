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

#include "base/types/wrapper_type.h"

#include "absl/base/attributes.h"
#include "absl/base/call_once.h"
#include "absl/base/optimization.h"

namespace cel {

template class Handle<WrapperType>;
template class Handle<BoolWrapperType>;
template class Handle<BytesWrapperType>;
template class Handle<DoubleWrapperType>;
template class Handle<IntWrapperType>;
template class Handle<StringWrapperType>;
template class Handle<UintWrapperType>;

namespace {

ABSL_CONST_INIT absl::once_flag instance_once;

#define WRAPPER_STORAGE_NAME(name) name##_instance_storage

#define WRAPPER_STORAGE(type, name)                             \
  alignas(Handle<type##WrapperType>) char WRAPPER_STORAGE_NAME( \
      name)[sizeof(Handle<type##WrapperType>)]
WRAPPER_STORAGE(Bool, bool);
WRAPPER_STORAGE(Int, int);
WRAPPER_STORAGE(Uint, uint);
WRAPPER_STORAGE(Double, double);
WRAPPER_STORAGE(Bytes, bytes);
WRAPPER_STORAGE(String, string);
#undef WRAPPER_STORAGE

void InitializeWrapperTypes() {
  absl::call_once(instance_once, []() {
#define WRAPPER_MAKE_AT(type, name)                                           \
  base_internal::HandleFactory<type##WrapperType>::MakeAt<type##WrapperType>( \
      &WRAPPER_STORAGE_NAME(name))
    WRAPPER_MAKE_AT(Bool, bool);
    WRAPPER_MAKE_AT(Int, int);
    WRAPPER_MAKE_AT(Uint, uint);
    WRAPPER_MAKE_AT(Double, double);
    WRAPPER_MAKE_AT(Bytes, bytes);
    WRAPPER_MAKE_AT(String, string);
#undef WRAPPER_MAKE_AT
  });
}

}  // namespace

absl::string_view WrapperType::name() const {
  switch (base_internal::Metadata::GetInlineVariant<Kind>(*this)) {
    case Kind::kBool:
      return static_cast<const BoolWrapperType*>(this)->name();
    case Kind::kBytes:
      return static_cast<const BytesWrapperType*>(this)->name();
    case Kind::kDouble:
      return static_cast<const DoubleWrapperType*>(this)->name();
    case Kind::kInt:
      return static_cast<const IntWrapperType*>(this)->name();
    case Kind::kString:
      return static_cast<const StringWrapperType*>(this)->name();
    case Kind::kUint:
      return static_cast<const UintWrapperType*>(this)->name();
    default:
      // There are only 6 wrapper types.
      ABSL_UNREACHABLE();
  }
}

const Handle<Type>& WrapperType::wrapped() const {
  switch (base_internal::Metadata::GetInlineVariant<Kind>(*this)) {
    case Kind::kBool:
      return static_cast<const BoolWrapperType*>(this)->wrapped().As<Type>();
    case Kind::kBytes:
      return static_cast<const BytesWrapperType*>(this)->wrapped().As<Type>();
    case Kind::kDouble:
      return static_cast<const DoubleWrapperType*>(this)->wrapped().As<Type>();
    case Kind::kInt:
      return static_cast<const IntWrapperType*>(this)->wrapped().As<Type>();
    case Kind::kString:
      return static_cast<const StringWrapperType*>(this)->wrapped().As<Type>();
    case Kind::kUint:
      return static_cast<const UintWrapperType*>(this)->wrapped().As<Type>();
    default:
      // There are only 6 wrapper types.
      ABSL_UNREACHABLE();
  }
}

const Handle<BoolWrapperType>& BoolWrapperType::Get() {
  InitializeWrapperTypes();
  return *reinterpret_cast<const Handle<BoolWrapperType>*>(
      WRAPPER_STORAGE_NAME(bool));
}

const Handle<BytesWrapperType>& BytesWrapperType::Get() {
  InitializeWrapperTypes();
  return *reinterpret_cast<const Handle<BytesWrapperType>*>(
      WRAPPER_STORAGE_NAME(bytes));
}

const Handle<DoubleWrapperType>& DoubleWrapperType::Get() {
  InitializeWrapperTypes();
  return *reinterpret_cast<const Handle<DoubleWrapperType>*>(
      WRAPPER_STORAGE_NAME(double));
}

const Handle<IntWrapperType>& IntWrapperType::Get() {
  InitializeWrapperTypes();
  return *reinterpret_cast<const Handle<IntWrapperType>*>(
      WRAPPER_STORAGE_NAME(int));
}

const Handle<StringWrapperType>& StringWrapperType::Get() {
  InitializeWrapperTypes();
  return *reinterpret_cast<const Handle<StringWrapperType>*>(
      WRAPPER_STORAGE_NAME(string));
}

const Handle<UintWrapperType>& UintWrapperType::Get() {
  InitializeWrapperTypes();
  return *reinterpret_cast<const Handle<UintWrapperType>*>(
      WRAPPER_STORAGE_NAME(uint));
}

#undef WRAPPER_STORAGE_NAME

}  // namespace cel
