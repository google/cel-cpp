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

#include "common/internal/managed_byte_string.h"

#include <string>
#include <utility>

#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/internal/reference_count.h"
#include "internal/testing.h"

namespace cel::common_internal {
namespace {

using testing::Eq;
using testing::IsEmpty;
using testing::Ne;
using testing::Not;

class OwningObject final : public ReferenceCount {
 public:
  explicit OwningObject(std::string string) : string_(std::move(string)) {}

  absl::string_view owned_string() const { return string_; }

 private:
  void Finalize() const noexcept override { std::string().swap(string_); }

  mutable std::string string_;
};

TEST(ManagedByteString, DefaultConstructor) {
  ManagedByteString byte_string;
  std::string scratch;
  EXPECT_THAT(byte_string.ToString(scratch), IsEmpty());
  EXPECT_THAT(byte_string.ToCord(), IsEmpty());
}

TEST(ManagedByteString, StringView) {
  absl::string_view string_view = "foo";
  ManagedByteString byte_string(string_view);
  std::string scratch;
  EXPECT_THAT(byte_string.ToString(scratch), Not(IsEmpty()));
  EXPECT_THAT(byte_string.ToString(scratch).data(), Eq(string_view.data()));
  auto cord = byte_string.ToCord();
  EXPECT_THAT(cord, Eq("foo"));
  EXPECT_THAT(cord.Flatten().data(), Ne(string_view.data()));
}

TEST(ManagedByteString, OwnedStringView) {
  auto* const owner =
      new OwningObject("----------------------------------------");
  {
    ManagedByteString byte_string1(owner, owner->owned_string());
    ManagedByteStringView byte_string2(byte_string1);
    ManagedByteString byte_string3(byte_string2);
    std::string scratch;
    EXPECT_THAT(byte_string3.ToString(scratch), Not(IsEmpty()));
    EXPECT_THAT(byte_string3.ToString(scratch).data(),
                Eq(owner->owned_string().data()));
    auto cord = byte_string3.ToCord();
    EXPECT_THAT(cord, Eq(owner->owned_string()));
    EXPECT_THAT(cord.Flatten().data(), Eq(owner->owned_string().data()));
  }
  StrongUnref(owner);
}

TEST(ManagedByteString, String) {
  ManagedByteString byte_string(std::string("foo"));
  std::string scratch;
  EXPECT_THAT(byte_string.ToString(scratch), Eq("foo"));
  EXPECT_THAT(byte_string.ToCord(), Eq("foo"));
}

TEST(ManagedByteString, Cord) {
  ManagedByteString byte_string(absl::Cord("foo"));
  std::string scratch;
  EXPECT_THAT(byte_string.ToString(scratch), Eq("foo"));
  EXPECT_THAT(byte_string.ToCord(), Eq("foo"));
}

TEST(ManagedByteString, CopyConstruct) {
  ManagedByteString byte_string1(absl::string_view("foo"));
  ManagedByteString byte_string2(std::string("bar"));
  ManagedByteString byte_string3(absl::Cord("baz"));
  EXPECT_THAT(ManagedByteString(byte_string1).ToString(),
              byte_string1.ToString());
  EXPECT_THAT(ManagedByteString(byte_string2).ToString(),
              byte_string2.ToString());
  EXPECT_THAT(ManagedByteString(byte_string3).ToString(),
              byte_string3.ToString());
}

TEST(ManagedByteString, MoveConstruct) {
  ManagedByteString byte_string1(absl::string_view("foo"));
  ManagedByteString byte_string2(std::string("bar"));
  ManagedByteString byte_string3(absl::Cord("baz"));
  EXPECT_THAT(ManagedByteString(std::move(byte_string1)).ToString(), Eq("foo"));
  EXPECT_THAT(ManagedByteString(std::move(byte_string2)).ToString(), Eq("bar"));
  EXPECT_THAT(ManagedByteString(std::move(byte_string3)).ToString(), Eq("baz"));
}

TEST(ManagedByteString, CopyAssign) {
  ManagedByteString byte_string1(absl::string_view("foo"));
  ManagedByteString byte_string2(std::string("bar"));
  ManagedByteString byte_string3(absl::Cord("baz"));
  ManagedByteString byte_string;
  EXPECT_THAT((byte_string = byte_string1).ToString(), byte_string1.ToString());
  EXPECT_THAT((byte_string = byte_string2).ToString(), byte_string2.ToString());
  EXPECT_THAT((byte_string = byte_string3).ToString(), byte_string3.ToString());
}

TEST(ManagedByteString, MoveAssign) {
  ManagedByteString byte_string1(absl::string_view("foo"));
  ManagedByteString byte_string2(std::string("bar"));
  ManagedByteString byte_string3(absl::Cord("baz"));
  ManagedByteString byte_string;
  EXPECT_THAT((byte_string = std::move(byte_string1)).ToString(), Eq("foo"));
  EXPECT_THAT((byte_string = std::move(byte_string2)).ToString(), Eq("bar"));
  EXPECT_THAT((byte_string = std::move(byte_string3)).ToString(), Eq("baz"));
}

TEST(ManagedByteString, Swap) {
  ManagedByteString byte_string1(absl::string_view("foo"));
  ManagedByteString byte_string2(std::string("bar"));
  ManagedByteString byte_string3(absl::Cord("baz"));
  ManagedByteString byte_string4;
  byte_string1.swap(byte_string2);
  byte_string2.swap(byte_string3);
  byte_string2.swap(byte_string3);
  byte_string2.swap(byte_string3);
  byte_string4 = byte_string1;
  byte_string1.swap(byte_string4);
  byte_string4 = byte_string2;
  byte_string2.swap(byte_string4);
  byte_string4 = byte_string3;
  byte_string3.swap(byte_string4);
  EXPECT_THAT(byte_string1.ToString(), Eq("bar"));
  EXPECT_THAT(byte_string2.ToString(), Eq("baz"));
  EXPECT_THAT(byte_string3.ToString(), Eq("foo"));
}

TEST(ManagedByteString, ManagedByteStringView) {
  ManagedByteString byte_string1(absl::string_view("foo"));
  ManagedByteString byte_string2(std::string("bar"));
  ManagedByteString byte_string3(absl::Cord("baz"));
  EXPECT_THAT(ManagedByteStringView(byte_string1).ToString(), Eq("foo"));
  EXPECT_THAT(ManagedByteStringView(byte_string2).ToString(), Eq("bar"));
  EXPECT_THAT(ManagedByteStringView(byte_string3).ToString(), Eq("baz"));
}

TEST(ManagedByteStringView, DefaultConstructor) {
  ManagedByteStringView byte_string;
  std::string scratch;
  EXPECT_THAT(byte_string.ToString(scratch), IsEmpty());
  EXPECT_THAT(byte_string.ToCord(), IsEmpty());
}

TEST(ManagedByteStringView, StringView) {
  absl::string_view string_view = "foo";
  ManagedByteStringView byte_string(string_view);
  std::string scratch;
  EXPECT_THAT(byte_string.ToString(scratch), Not(IsEmpty()));
  EXPECT_THAT(byte_string.ToString(scratch).data(), Eq(string_view.data()));
  auto cord = byte_string.ToCord();
  EXPECT_THAT(cord, Eq("foo"));
  EXPECT_THAT(cord.Flatten().data(), Ne(string_view.data()));
}

TEST(ManagedByteStringView, OwnedStringView) {
  auto* const owner =
      new OwningObject("----------------------------------------");
  {
    ManagedByteString byte_string1(owner, owner->owned_string());
    ManagedByteStringView byte_string2(byte_string1);
    std::string scratch;
    EXPECT_THAT(byte_string2.ToString(scratch), Not(IsEmpty()));
    EXPECT_THAT(byte_string2.ToString(scratch).data(),
                Eq(owner->owned_string().data()));
    auto cord = byte_string2.ToCord();
    EXPECT_THAT(cord, Eq(owner->owned_string()));
    EXPECT_THAT(cord.Flatten().data(), Eq(owner->owned_string().data()));
  }
  StrongUnref(owner);
}

TEST(ManagedByteStringView, String) {
  std::string string("foo");
  ManagedByteStringView byte_string(string);
  std::string scratch;
  EXPECT_THAT(byte_string.ToString(scratch), Eq("foo"));
  EXPECT_THAT(byte_string.ToCord(), Eq("foo"));
}

TEST(ManagedByteStringView, Cord) {
  absl::Cord cord("foo");
  ManagedByteStringView byte_string(cord);
  std::string scratch;
  EXPECT_THAT(byte_string.ToString(scratch), Eq("foo"));
  EXPECT_THAT(byte_string.ToCord(), Eq("foo"));
}

TEST(ManagedByteStringView, CopyConstruct) {
  std::string string("bar");
  absl::Cord cord("baz");
  ManagedByteStringView byte_string1(absl::string_view("foo"));
  ManagedByteStringView byte_string2(string);
  ManagedByteStringView byte_string3(cord);
  EXPECT_THAT(ManagedByteString(byte_string1).ToString(),
              byte_string1.ToString());
  EXPECT_THAT(ManagedByteString(byte_string2).ToString(),
              byte_string2.ToString());
  EXPECT_THAT(ManagedByteString(byte_string3).ToString(),
              byte_string3.ToString());
}

TEST(ManagedByteStringView, MoveConstruct) {
  std::string string("bar");
  absl::Cord cord("baz");
  ManagedByteStringView byte_string1(absl::string_view("foo"));
  ManagedByteStringView byte_string2(string);
  ManagedByteStringView byte_string3(cord);
  EXPECT_THAT(ManagedByteString(std::move(byte_string1)).ToString(), Eq("foo"));
  EXPECT_THAT(ManagedByteString(std::move(byte_string2)).ToString(), Eq("bar"));
  EXPECT_THAT(ManagedByteString(std::move(byte_string3)).ToString(), Eq("baz"));
}

TEST(ManagedByteStringView, CopyAssign) {
  std::string string("bar");
  absl::Cord cord("baz");
  ManagedByteStringView byte_string1(absl::string_view("foo"));
  ManagedByteStringView byte_string2(string);
  ManagedByteStringView byte_string3(cord);
  ManagedByteStringView byte_string;
  EXPECT_THAT((byte_string = byte_string1).ToString(), byte_string1.ToString());
  EXPECT_THAT((byte_string = byte_string2).ToString(), byte_string2.ToString());
  EXPECT_THAT((byte_string = byte_string3).ToString(), byte_string3.ToString());
}

TEST(ManagedByteStringView, MoveAssign) {
  std::string string("bar");
  absl::Cord cord("baz");
  ManagedByteStringView byte_string1(absl::string_view("foo"));
  ManagedByteStringView byte_string2(string);
  ManagedByteStringView byte_string3(cord);
  ManagedByteStringView byte_string;
  EXPECT_THAT((byte_string = std::move(byte_string1)).ToString(), Eq("foo"));
  EXPECT_THAT((byte_string = std::move(byte_string2)).ToString(), Eq("bar"));
  EXPECT_THAT((byte_string = std::move(byte_string3)).ToString(), Eq("baz"));
}

TEST(ManagedByteStringView, Swap) {
  std::string string("bar");
  absl::Cord cord("baz");
  ManagedByteStringView byte_string1(absl::string_view("foo"));
  ManagedByteStringView byte_string2(string);
  ManagedByteStringView byte_string3(cord);
  byte_string1.swap(byte_string2);
  byte_string2.swap(byte_string3);
  EXPECT_THAT(byte_string1.ToString(), Eq("bar"));
  EXPECT_THAT(byte_string2.ToString(), Eq("baz"));
  EXPECT_THAT(byte_string3.ToString(), Eq("foo"));
}

TEST(ManagedByteStringView, ManagedByteString) {
  std::string string("bar");
  absl::Cord cord("baz");
  ManagedByteStringView byte_string1(absl::string_view("foo"));
  ManagedByteStringView byte_string2(string);
  ManagedByteStringView byte_string3(cord);
  EXPECT_THAT(ManagedByteString(byte_string1).ToString(), Eq("foo"));
  EXPECT_THAT(ManagedByteString(byte_string2).ToString(), Eq("bar"));
  EXPECT_THAT(ManagedByteString(byte_string3).ToString(), Eq("baz"));
}

}  // namespace
}  // namespace cel::common_internal
