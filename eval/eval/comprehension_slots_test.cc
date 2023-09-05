// Copyright 2023 Google LLC
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

#include "eval/eval/comprehension_slots.h"

#include "base/attribute.h"
#include "base/handle.h"
#include "base/memory.h"
#include "base/type.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "base/type_provider.h"
#include "base/value_factory.h"
#include "base/values/string_value.h"
#include "eval/eval/attribute_trail.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

using ::cel::Attribute;
using ::cel::Handle;
using ::cel::MemoryManager;
using ::cel::StringValue;
using ::cel::TypeFactory;
using ::cel::TypeManager;
using ::cel::TypeProvider;
using ::cel::Value;
using ::cel::ValueFactory;
using testing::Truly;
using cel::internal::IsOkAndHolds;

TEST(ComprehensionSlots, Basic) {
  TypeFactory tf(MemoryManager::Global());
  TypeManager tm(tf, TypeProvider::Builtin());
  ValueFactory factory(tm);

  ComprehensionSlots slots(4);

  ComprehensionSlots::Slot* unset = slots.Get(0);
  EXPECT_EQ(unset, nullptr);

  slots.Set(0, factory.CreateUncheckedStringValue("abcd"),
            AttributeTrail(Attribute("fake_attr")));

  auto* slot0 = slots.Get(0);
  ASSERT_TRUE(slot0 != nullptr);

  EXPECT_THAT(slot0->value, Truly([](const Handle<Value>& v) {
                return v->Is<StringValue>() &&
                       v->As<StringValue>().ToString() == "abcd";
              }))
      << "value is 'abcd'";

  EXPECT_THAT(slot0->attribute.attribute().AsString(),
              IsOkAndHolds("fake_attr"));

  slots.ClearSlot(0);
  EXPECT_EQ(slots.Get(0), nullptr);

  slots.Set(3, factory.CreateUncheckedStringValue("abcd"),
            AttributeTrail(Attribute("fake_attr")));

  auto* slot3 = slots.Get(3);

  ASSERT_TRUE(slot3 != nullptr);
  EXPECT_THAT(slot3->value, Truly([](const Handle<Value>& v) {
                return v->Is<StringValue>() &&
                       v->As<StringValue>().ToString() == "abcd";
              }))
      << "value is 'abcd'";

  slots.Reset();
  slot0 = slots.Get(0);
  EXPECT_TRUE(slot0 == nullptr);
  slot3 = slots.Get(3);
  EXPECT_TRUE(slot3 == nullptr);
}

}  // namespace google::api::expr::runtime
