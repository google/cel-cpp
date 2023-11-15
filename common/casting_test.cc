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

#include "common/casting.h"

#include <memory>
#include <type_traits>

#include "absl/types/optional.h"
#include "common/native_type.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::_;
using testing::Eq;
using testing::Ne;
using testing::Ref;

enum class AncestryKind {
  kParent,
  kChild,
};

class Ancestry {
 public:
  virtual ~Ancestry() = default;

  virtual AncestryKind kind() const = 0;

 private:
  friend struct NativeTypeTraits<Ancestry>;

  virtual NativeTypeId GetNativeTypeId() const = 0;
};

}  // namespace

// Specialize `NativeTypeTraits` for `Ancestry`. Enables `NativeTypeId::Of`.
template <>
struct NativeTypeTraits<Ancestry> final {
  static NativeTypeId Id(const Ancestry& ancestry) {
    return ancestry.GetNativeTypeId();
  }
};

// Specialize `NativeTypeTraits` for types derived from `Ancestry`, deferring to
// the above specialization for the implementation of `Id`.
template <typename T>
struct NativeTypeTraits<T, std::enable_if_t<std::conjunction_v<
                               std::is_base_of<Ancestry, T>,
                               std::negation<std::is_same<T, Ancestry>>>>>
    final {
  static NativeTypeId Id(const Ancestry& ancestry) {
    return NativeTypeTraits<Ancestry>::Id(ancestry);
  }
};

// Enable `InstanceOf`, `Cast`, and `As` using subsumption relationships.
// `SubsumptionTraits` is defined for derived classes below.
template <typename To, typename From>
struct CastTraits<To, From, EnableIfSubsumptionCastable<To, From, Ancestry>>
    : SubsumptionCastTraits<To, From> {};

namespace {

class Parent final : public Ancestry {
 public:
  AncestryKind kind() const override { return AncestryKind::kParent; }

 private:
  // Friend is only needed if the specialization needs to access private
  // members. Here we do not need it, but do it as typical implementations may
  // require it.
  friend struct SubsumptionTraits<Parent>;

  NativeTypeId GetNativeTypeId() const override {
    return NativeTypeId::For<Parent>();
  }
};

class Child final : public Ancestry {
 public:
  AncestryKind kind() const override { return AncestryKind::kChild; }

 private:
  // Friend is only needed if the specialization needs to access private
  // members. Here we do not need it, but do it as typical implementations may
  // require it.
  friend struct SubsumptionTraits<Child>;

  NativeTypeId GetNativeTypeId() const override {
    return NativeTypeId::For<Child>();
  }
};

}  // namespace

// Specialize `SubsumptionTraits` for `Parent`. Used by `SubsumptionCastTraits`.
template <>
struct SubsumptionTraits<Parent> final {
  static bool IsA(const Ancestry& ancestry) {
    return ancestry.kind() == AncestryKind::kParent;
  }
};

// Specialize `SubsumptionTraits` for `Child`. Used by `SubsumptionCastTraits`.
template <>
struct SubsumptionTraits<Child> final {
  static bool IsA(const Ancestry& ancestry) {
    return ancestry.kind() == AncestryKind::kChild;
  }
};

namespace {

inline bool operator==(const Ancestry& lhs, const Ancestry& rhs) {
  return lhs.kind() == rhs.kind();
}

inline bool operator!=(const Ancestry& lhs, const Ancestry& rhs) {
  return !operator==(lhs, rhs);
}

TEST(InstanceOf, Same) {
  Child child;
  EXPECT_TRUE(InstanceOf<Child>(child));
}

TEST(InstanceOf, Supertype) {
  Child child;
  EXPECT_TRUE(InstanceOf<Ancestry>(child));
}

TEST(InstanceOf, Subtype) {
  std::unique_ptr<Ancestry> ancestry = std::make_unique<Parent>();
  EXPECT_FALSE(InstanceOf<Child>(*ancestry));
}

TEST(Cast, Same) {
  Child child;
  EXPECT_THAT(Cast<Child>(child), Ref(child));
}

TEST(Cast, Supertype) {
  Child child;
  EXPECT_THAT(Cast<Ancestry>(child), Ref(child));
}

TEST(Cast, Subtype) {
  std::unique_ptr<Ancestry> ancestry = std::make_unique<Parent>();
  EXPECT_DEBUG_DEATH(static_cast<void>(Cast<Child>(*ancestry)), _);
}

TEST(As, Same) {
  Child child;
  EXPECT_THAT(As<Child>(child), Ne(absl::nullopt));
}

TEST(As, Supertype) {
  Child child;
  EXPECT_THAT(As<Ancestry>(child), Ne(absl::nullopt));
}

TEST(As, Subtype) {
  std::unique_ptr<Ancestry> ancestry = std::make_unique<Parent>();
  EXPECT_THAT(As<Child>(*ancestry), Eq(absl::nullopt));
}

}  // namespace
}  // namespace cel
