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
#include "common/casting.h"
#include "common/memory.h"
#include "common/memory_testing.h"
#include "common/type_factory.h"
#include "common/type_introspector.h"
#include "common/type_manager.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "common/value_manager.h"

namespace cel::testing {

MATCHER_P(ValueKindIs, m, "") {
  return ExplainMatchResult(m, arg.kind(), result_listener);
}

MATCHER_P(BoolValueIs, m, "") {
  if (!cel::InstanceOf<BoolValueView>(cel::ValueView(arg))) {
    return false;
  }
  return ExplainMatchResult(m, cel::Cast<BoolValueView>(cel::ValueView(arg)),
                            result_listener);
}

MATCHER_P(IntValueIs, m, "") {
  if (!cel::InstanceOf<IntValueView>(cel::ValueView(arg))) {
    return false;
  }
  return ExplainMatchResult(m, cel::Cast<IntValueView>(cel::ValueView(arg)),
                            result_listener);
}

MATCHER_P(UintValueIs, m, "") {
  if (!cel::InstanceOf<UintValueView>(cel::ValueView(arg))) {
    return false;
  }
  return ExplainMatchResult(m, cel::Cast<UintValueView>(cel::ValueView(arg)),
                            result_listener);
}

MATCHER_P(DoubleValueIs, m, "") {
  if (!cel::InstanceOf<DoubleValueView>(cel::ValueView(arg))) {
    return false;
  }
  return ExplainMatchResult(m, cel::Cast<DoubleValueView>(cel::ValueView(arg)),
                            result_listener);
}

MATCHER_P(BytesValueIs, m, "") {
  if (!cel::InstanceOf<BytesValueView>(cel::ValueView(arg))) {
    return false;
  }
  return ExplainMatchResult(m, cel::Cast<BytesValueView>(cel::ValueView(arg)),
                            result_listener);
}

MATCHER_P(StringValueIs, m, "") {
  if (!cel::InstanceOf<StringValueView>(cel::ValueView(arg))) {
    return false;
  }
  return ExplainMatchResult(m, cel::Cast<StringValueView>(cel::ValueView(arg)),
                            result_listener);
}

MATCHER_P(DurationValueIs, m, "") {
  if (!cel::InstanceOf<DurationValueView>(cel::ValueView(arg))) {
    return false;
  }
  return ExplainMatchResult(
      m, cel::Cast<DurationValueView>(cel::ValueView(arg)), result_listener);
}

MATCHER_P(TimestampValueIs, m, "") {
  if (!cel::InstanceOf<TimestampValueView>(cel::ValueView(arg))) {
    return false;
  }
  return ExplainMatchResult(
      m, cel::Cast<TimestampValueView>(cel::ValueView(arg)), result_listener);
}

MATCHER_P(StructValueIs, m, "") {
  if (!cel::InstanceOf<StructValueView>(cel::ValueView(arg))) {
    return false;
  }
  return ExplainMatchResult(m, cel::Cast<StructValueView>(cel::ValueView(arg)),
                            result_listener);
}

MATCHER_P3(StructValueFieldIs, mgr, name, m, "") {
  return ExplainMatchResult(
      ::testing::status::IsOkAndHolds(m),
      cel::StructValueView(arg).GetFieldByName(*mgr, name), result_listener);
}

MATCHER_P2(StructValueFieldHas, name, m, "") {
  return ExplainMatchResult(::testing::status::IsOkAndHolds(m),
                            cel::StructValueView(arg).HasFieldByName(name),
                            result_listener);
}

}  // namespace cel::testing

namespace cel::common_internal {

template <typename... Ts>
class ThreadCompatibleValueTest : public ThreadCompatibleMemoryTest<Ts...> {
 private:
  using Base = ThreadCompatibleMemoryTest<Ts...>;

 public:
  void SetUp() override {
    Base::SetUp();
    value_manager_ = NewThreadCompatibleValueManager(
        this->memory_manager(),
        NewThreadCompatibleTypeReflector(this->memory_manager()));
  }

  void TearDown() override {
    value_manager_.reset();
    Base::TearDown();
  }

  ValueManager& value_manager() const { return **value_manager_; }

  TypeFactory& type_factory() const { return value_manager(); }

  TypeManager& type_manager() const { return value_manager(); }

  ValueFactory& value_factory() const { return value_manager(); }

 private:
  absl::optional<Shared<ValueManager>> value_manager_;
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUE_TESTING_H_
