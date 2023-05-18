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

#include <cstdint>

#include "absl/base/attributes.h"
#include "absl/base/call_once.h"
#include "base/handle.h"
#include "base/types/any_type.h"
#include "base/types/bool_type.h"
#include "base/types/bytes_type.h"
#include "base/types/double_type.h"
#include "base/types/duration_type.h"
#include "base/types/dyn_type.h"
#include "base/types/error_type.h"
#include "base/types/int_type.h"
#include "base/types/null_type.h"
#include "base/types/string_type.h"
#include "base/types/timestamp_type.h"
#include "base/types/type_type.h"
#include "base/types/uint_type.h"
#include "base/types/unknown_type.h"
#include "base/types/wrapper_type.h"

namespace cel {

namespace {

ABSL_CONST_INIT absl::once_flag types_once;

#define TYPE_STORAGE_NAME(type) type##_storage
#define TYPE_AT(type) \
  *reinterpret_cast<const Handle<type>*>(&TYPE_STORAGE_NAME(type)[0])

#define TYPES(XX)       \
  XX(DynType)           \
  XX(AnyType)           \
  XX(BoolType)          \
  XX(BytesType)         \
  XX(DoubleType)        \
  XX(DurationType)      \
  XX(ErrorType)         \
  XX(IntType)           \
  XX(NullType)          \
  XX(StringType)        \
  XX(TimestampType)     \
  XX(TypeType)          \
  XX(UintType)          \
  XX(UnknownType)       \
  XX(BoolWrapperType)   \
  XX(IntWrapperType)    \
  XX(UintWrapperType)   \
  XX(DoubleWrapperType) \
  XX(BytesWrapperType)  \
  XX(StringWrapperType)

#define TYPE_STORAGE(type) \
  alignas(Handle<type>) uint8_t TYPE_STORAGE_NAME(type)[sizeof(Handle<type>)];

TYPES(TYPE_STORAGE)

#undef TYPE_STORAGE

#define TYPE_MAKE_AT(type) \
  base_internal::HandleFactory<type>::MakeAt<type>(&TYPE_STORAGE_NAME(type)[0]);

void InitializeTypes() { TYPES(TYPE_MAKE_AT) }

#undef TYPE_MAKE_AT

}  // namespace

#define TYPE_GET(type)                            \
  const Handle<type>& type::Get() {               \
    absl::call_once(types_once, InitializeTypes); \
    return TYPE_AT(type);                         \
  }
TYPES(TYPE_GET)
#undef TYPE_GET

#undef TYPES
#undef TYPE_AT
#undef TYPE_STORAGE_NAME

}  // namespace cel
