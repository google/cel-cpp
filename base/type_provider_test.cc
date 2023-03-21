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

#include "base/type_provider.h"

#include "base/memory_manager.h"
#include "base/type_factory.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::Eq;
using testing::Optional;
using cel::internal::IsOkAndHolds;

TEST(BuiltinTypeProvider, ProvidesBoolWrapperType) {
  TypeFactory type_factory(MemoryManager::Global());
  ASSERT_THAT(TypeProvider::Builtin().ProvideType(type_factory,
                                                  "google.protobuf.BoolValue"),
              IsOkAndHolds(Optional(Eq(type_factory.GetBoolWrapperType()))));
}

TEST(BuiltinTypeProvider, ProvidesBytesWrapperType) {
  TypeFactory type_factory(MemoryManager::Global());
  ASSERT_THAT(TypeProvider::Builtin().ProvideType(type_factory,
                                                  "google.protobuf.BytesValue"),
              IsOkAndHolds(Optional(Eq(type_factory.GetBytesWrapperType()))));
}

TEST(BuiltinTypeProvider, ProvidesDoubleWrapperType) {
  TypeFactory type_factory(MemoryManager::Global());
  ASSERT_THAT(TypeProvider::Builtin().ProvideType(type_factory,
                                                  "google.protobuf.FloatValue"),
              IsOkAndHolds(Optional(Eq(type_factory.GetDoubleWrapperType()))));
  ASSERT_THAT(TypeProvider::Builtin().ProvideType(
                  type_factory, "google.protobuf.DoubleValue"),
              IsOkAndHolds(Optional(Eq(type_factory.GetDoubleWrapperType()))));
}

TEST(BuiltinTypeProvider, ProvidesIntWrapperType) {
  TypeFactory type_factory(MemoryManager::Global());
  ASSERT_THAT(TypeProvider::Builtin().ProvideType(type_factory,
                                                  "google.protobuf.Int32Value"),
              IsOkAndHolds(Optional(Eq(type_factory.GetIntWrapperType()))));
  ASSERT_THAT(TypeProvider::Builtin().ProvideType(type_factory,
                                                  "google.protobuf.Int64Value"),
              IsOkAndHolds(Optional(Eq(type_factory.GetIntWrapperType()))));
}

TEST(BuiltinTypeProvider, ProvidesStringWrapperType) {
  TypeFactory type_factory(MemoryManager::Global());
  ASSERT_THAT(TypeProvider::Builtin().ProvideType(
                  type_factory, "google.protobuf.StringValue"),
              IsOkAndHolds(Optional(Eq(type_factory.GetStringWrapperType()))));
}

TEST(BuiltinTypeProvider, ProvidesUintWrapperType) {
  TypeFactory type_factory(MemoryManager::Global());
  ASSERT_THAT(TypeProvider::Builtin().ProvideType(
                  type_factory, "google.protobuf.UInt32Value"),
              IsOkAndHolds(Optional(Eq(type_factory.GetUintWrapperType()))));
  ASSERT_THAT(TypeProvider::Builtin().ProvideType(
                  type_factory, "google.protobuf.UInt64Value"),
              IsOkAndHolds(Optional(Eq(type_factory.GetUintWrapperType()))));
}

}  // namespace
}  // namespace cel
