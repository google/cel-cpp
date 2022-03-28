// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_BASE_TYPE_MANAGER_H_
#define THIRD_PARTY_CEL_CPP_BASE_TYPE_MANAGER_H_

#include "base/type_factory.h"
#include "base/type_registry.h"

namespace cel {

// TypeManager is a union of the TypeFactory and TypeRegistry, allowing for both
// the instantiation of type implementations, loading of type implementations,
// and registering type implementations.
//
// TODO(issues/5): more comments after solidifying role
class TypeManager : public TypeFactory, public TypeRegistry {
 public:
  using TypeFactory::TypeFactory;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPE_MANAGER_H_
