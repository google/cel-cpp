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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPE_REFLECTOR_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPE_REFLECTOR_H_

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/type_introspector.h"
#include "common/value.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/message.h"

namespace cel {

// `TypeReflector` is an interface for constructing new instances of types are
// runtime. It handles type reflection.
class TypeReflector : public virtual TypeIntrospector {
 public:
  // `NewValueBuilder` returns a new `ValueBuilder` for the corresponding type
  // `name`.  It is primarily used to handle wrapper types which sometimes show
  // up literally in expressions.
  virtual absl::StatusOr<absl::Nullable<ValueBuilderPtr>> NewValueBuilder(
      absl::string_view name,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena) const = 0;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPE_REFLECTOR_H_
