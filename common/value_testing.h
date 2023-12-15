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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUE_TESTING_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUE_TESTING_H_

#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/memory_testing.h"
#include "common/type_factory.h"
#include "common/type_provider.h"
#include "common/value_factory.h"
#include "common/value_provider.h"

namespace cel::common_internal {

template <typename... Ts>
class ThreadCompatibleValueTest : public ThreadCompatibleMemoryTest<Ts...> {
 private:
  using Base = ThreadCompatibleMemoryTest<Ts...>;

 public:
  void SetUp() override {
    Base::SetUp();
    value_factory_ = NewThreadCompatibleValueFactory(this->memory_manager());
    value_provider_ = NewThreadCompatibleValueProvider(this->memory_manager());
  }

  void TearDown() override {
    value_provider_.reset();
    value_factory_.reset();
    Base::TearDown();
  }

  TypeFactory& type_factory() const { return **value_factory_; }

  TypeProvider& type_provider() const { return **value_provider_; }

  ValueFactory& value_factory() const { return **value_factory_; }

  ValueProvider& value_provider() const { return **value_provider_; }

 private:
  absl::optional<Shared<ValueFactory>> value_factory_;
  absl::optional<Shared<ValueProvider>> value_provider_;
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUE_TESTING_H_
