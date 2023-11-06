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

#include "common/internal/reference_count.h"

#include <tuple>

#include "internal/testing.h"

namespace cel::common_internal {
namespace {

class Object : public virtual ReferenceCountFromThis {
 public:
  explicit Object(bool& destructed) : destructed_(destructed) {}

  ~Object() { destructed_ = true; }

 private:
  bool& destructed_;
};

class Subobject : public Object, public virtual ReferenceCountFromThis {
 public:
  using Object::Object;
};

TEST(ReferenceCount, Strong) {
  bool destructed = false;
  Object* object;
  ReferenceCount* refcount;
  std::tie(object, refcount) = MakeReferenceCount<Subobject>(destructed);
  EXPECT_EQ(GetReferenceCountForThat(*object), refcount);
  EXPECT_EQ(GetReferenceCountForThat(*static_cast<Subobject*>(object)),
            refcount);
  StrongRef(refcount);
  StrongUnref(refcount);
  EXPECT_TRUE(IsUniqueRef(refcount));
  EXPECT_FALSE(destructed);
  StrongUnref(refcount);
  EXPECT_TRUE(destructed);
}

TEST(ReferenceCount, Weak) {
  bool destructed = false;
  Object* object;
  ReferenceCount* refcount;
  std::tie(object, refcount) = MakeReferenceCount<Subobject>(destructed);
  EXPECT_EQ(GetReferenceCountForThat(*object), refcount);
  EXPECT_EQ(GetReferenceCountForThat(*static_cast<Subobject*>(object)),
            refcount);
  WeakRef(refcount);
  ASSERT_TRUE(StrengthenRef(refcount));
  StrongUnref(refcount);
  EXPECT_TRUE(IsUniqueRef(refcount));
  EXPECT_FALSE(destructed);
  StrongUnref(refcount);
  EXPECT_TRUE(destructed);
  ASSERT_FALSE(StrengthenRef(refcount));
  WeakUnref(refcount);
}

}  // namespace
}  // namespace cel::common_internal
