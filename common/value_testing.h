// Copyright 2024 Google LLC
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

#include <cstdint>
#include <ostream>
#include <string>

#include "absl/status/status.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/memory_testing.h"
#include "common/type_factory.h"
#include "common/type_introspector.h"
#include "common/type_manager.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "common/value_kind.h"
#include "common/value_manager.h"
#include "internal/testing.h"

namespace cel {

// GTest Printer
void PrintTo(const Value& value, std::ostream* os);

namespace test {

using ValueMatcher = testing::Matcher<Value>;

MATCHER_P(ValueKindIs, m, "") {
  return ExplainMatchResult(m, arg.kind(), result_listener);
}

// Returns a matcher for CEL null value.
inline ValueMatcher IsNullValue() { return ValueKindIs(ValueKind::kNull); }

// Returns a matcher for CEL bool values.
ValueMatcher BoolValueIs(testing::Matcher<bool> m);

// Returns a matcher for CEL int values.
ValueMatcher IntValueIs(testing::Matcher<int64_t> m);

// Returns a matcher for CEL uint values.
ValueMatcher UintValueIs(testing::Matcher<uint64_t> m);

// Returns a matcher for CEL double values.
ValueMatcher DoubleValueIs(testing::Matcher<double> m);

// Returns a matcher for CEL duration values.
ValueMatcher DurationValueIs(testing::Matcher<absl::Duration> m);

// Returns a matcher for CEL timestamp values.
ValueMatcher TimestampValueIs(testing::Matcher<absl::Time> m);

// Returns a matcher for CEL error values.
ValueMatcher ErrorValueIs(testing::Matcher<absl::Status> m);

// Returns a matcher for CEL string values.
ValueMatcher StringValueIs(testing::Matcher<std::string> m);

// Returns a matcher for CEL bytes values.
ValueMatcher BytesValueIs(testing::Matcher<std::string> m);

// Returns a matcher for CEL map values.
ValueMatcher MapValueIs(testing::Matcher<MapValue> m);

// Returns a matcher for CEL list values.
ValueMatcher ListValueIs(testing::Matcher<ListValue> m);

// Returns a matcher for CEL struct values.
ValueMatcher StructValueIs(testing::Matcher<StructValue> m);

// Returns a Matcher that tests the value of a CEL struct's field.
// ValueManager* mgr must remain valid for the lifetime of the matcher.
MATCHER_P3(StructValueFieldIs, mgr, name, m, "") {
  auto wrapped_m = ::cel::internal::IsOkAndHolds(m);

  return ExplainMatchResult(
      wrapped_m, cel::StructValueView(arg).GetFieldByName(*mgr, name),
      result_listener);
}

// Returns a Matcher that tests the presence of a CEL struct's field.
// ValueManager* mgr must remain valid for the lifetime of the matcher.
MATCHER_P2(StructValueFieldHas, name, m, "") {
  auto wrapped_m = ::cel::internal::IsOkAndHolds(m);

  return ExplainMatchResult(wrapped_m,
                            cel::StructValueView(arg).HasFieldByName(name),
                            result_listener);
}

}  // namespace test

}  // namespace cel

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
