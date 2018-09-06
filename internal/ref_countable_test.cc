#include "internal/ref_countable.h"

#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "testutil/util.h"

namespace google {
namespace api {
namespace expr {
namespace internal {
namespace {

class TestRefCounted : public RefCountable {
 public:
  explicit TestRefCounted(int id) : id_(id) {}

  int id() const { return id_; }
  bool unowned() const { return RefCountable::unowned(); }
  bool single_owner() const { return RefCountable::single_owner(); }
  std::size_t owner_count() const { return RefCountable::owner_count(); }

 private:
  int id_;
  int data_[5];
};

TEST(RefCountableTest, StackValue) {
  TestRefCounted v1(1);

  EXPECT_TRUE(v1.unowned());
  EXPECT_FALSE(v1.single_owner());
  EXPECT_EQ(v1.id(), 1);
}

TEST(RefCountableTest, UnqiuePtr) {
  auto v1 = absl::make_unique<TestRefCounted>(1);

  EXPECT_TRUE(v1->unowned());
  EXPECT_FALSE(v1->single_owner());
  EXPECT_EQ(v1->id(), 1);
}

TEST(RefCountableTest, ReffedPtr) {
  auto v1 = MakeReffed<TestRefCounted>(1);

  EXPECT_FALSE(v1->unowned());
  EXPECT_TRUE(v1->single_owner());
  EXPECT_EQ(v1->id(), 1);
}

TEST(RefCountableTest, ReffedPtr_Eq) {
  auto v1 = MakeReffed<TestRefCounted>(1);
  auto v2 = v1;

  EXPECT_FALSE(v1->unowned());
  EXPECT_FALSE(v1->single_owner());
  EXPECT_EQ(2, v1->owner_count());
  EXPECT_EQ(v2->id(), 1);
}

TEST(RefCountableTest, ReffedPtr_Move) {
  auto v1 = MakeReffed<TestRefCounted>(1);
  auto v2 = std::move(v1);

  EXPECT_FALSE(v2->unowned());
  EXPECT_TRUE(v2->single_owner());
  EXPECT_EQ(1, v2->owner_count());
  EXPECT_EQ(v2->id(), 1);

  ReffedPtr<TestRefCounted> v3(std::move(v2));
  EXPECT_FALSE(v3->unowned());
  EXPECT_TRUE(v3->single_owner());
  EXPECT_EQ(1, v3->owner_count());
  EXPECT_EQ(v3->id(), 1);
}

TEST(RefCountableTest, ReffedPtr_CopyConstructed) {
  auto v1 = MakeReffed<TestRefCounted>(1);
  ReffedPtr<TestRefCounted> v2(v1);

  EXPECT_FALSE(v1->unowned());
  EXPECT_FALSE(v1->single_owner());
  EXPECT_EQ(2, v1->owner_count());
  EXPECT_EQ(v1->id(), 1);
}

TEST(RefCountableTest, ReffedPtr_PtrConstructed) {
  auto v1 = MakeReffed<TestRefCounted>(1);
  ReffedPtr<TestRefCounted> v2(&*v1);

  EXPECT_FALSE(v1->unowned());
  EXPECT_FALSE(v1->single_owner());
  EXPECT_EQ(2, v1->owner_count());
  EXPECT_EQ(v1->id(), 1);
}

TEST(RefCountableTest, ReffedPtr_Reset) {
  auto v1 = MakeReffed<TestRefCounted>(1);

  EXPECT_FALSE(v1->unowned());
  EXPECT_TRUE(v1->single_owner());
  EXPECT_EQ(v1->id(), 1);

  auto v2 = v1;
  // Can be constructed directly from the raw pointer with out breaking ref
  // counting.
  ReffedPtr<TestRefCounted> v3(&*v1);

  // They all point to the same object.
  EXPECT_EQ(&*v1, &*v2);
  EXPECT_EQ(&*v1, &*v3);
  EXPECT_FALSE(v3->unowned());
  EXPECT_FALSE(v3->single_owner());
  EXPECT_EQ(v3->id(), 1);
  EXPECT_EQ(3, v3->owner_count());

  v3.reset();
  EXPECT_FALSE(v1->unowned());
  EXPECT_FALSE(v1->single_owner());
  EXPECT_EQ(2, v1->owner_count());

  v2.reset();
  EXPECT_FALSE(v1->unowned());
  EXPECT_TRUE(v1->single_owner());
  EXPECT_EQ(1, v1->owner_count());
}

TEST(RefCountableTest, Copy) {
  TestRefCounted v1(1);
  TestRefCounted v2(2);
  TestRefCounted v3(v1);

  EXPECT_TRUE(v1.unowned());
  EXPECT_FALSE(v1.single_owner());
  EXPECT_EQ(v1.id(), 1);

  EXPECT_TRUE(v3.unowned());
  EXPECT_FALSE(v3.single_owner());
  EXPECT_EQ(v3.id(), 1);

  EXPECT_TRUE(v2.unowned());
  EXPECT_FALSE(v2.single_owner());
  EXPECT_EQ(v2.id(), 2);

  v2 = v1;
  EXPECT_TRUE(v2.unowned());
  EXPECT_FALSE(v2.single_owner());
  EXPECT_EQ(v2.id(), 1);
}

TEST(RefCountableTest, CopyFromPtr) {
  auto v1 = MakeReffed<TestRefCounted>(1);
  TestRefCounted v2(2);

  EXPECT_FALSE(v1->unowned());
  EXPECT_TRUE(v1->single_owner());
  EXPECT_EQ(v1->id(), 1);
  EXPECT_TRUE(v2.unowned());
  EXPECT_FALSE(v2.single_owner());
  EXPECT_EQ(v2.id(), 2);

  v2 = *v1;
  // Only the value is copied.
  EXPECT_FALSE(v1->unowned());
  EXPECT_TRUE(v1->single_owner());
  EXPECT_EQ(v1->id(), 1);
  EXPECT_TRUE(v2.unowned());
  EXPECT_FALSE(v2.single_owner());
  EXPECT_EQ(v2.id(), 1);
}

TEST(ReffedPtrTest, ConstConversion) {
  ReffedPtr<const TestRefCounted> const_ref = MakeReffed<TestRefCounted>(1);
  EXPECT_EQ(const_ref->id(), 1);
}

TEST(Ref, NotRefCountable) {
  using HolderType = Holder<int, Ref<Copy>>;
  EXPECT_TRUE(std::is_copy_constructible<HolderType>::value);
  EXPECT_TRUE(std::is_copy_assignable<HolderType>::value);
  EXPECT_TRUE(std::is_move_constructible<HolderType>::value);
  EXPECT_TRUE(std::is_move_assignable<HolderType>::value);

  // Value can be mutated.
  HolderType holder(1);
  testutil::ExpectSameType<int&, decltype(holder.value())>();
  EXPECT_EQ(1, holder.value());
  holder.value() = 2;
  EXPECT_EQ(2, holder.value());

  // Value is shared.
  HolderType holder2 = holder;
  EXPECT_EQ(&holder.value(), &holder2.value());

  // Const holder cannot be assigned or have its value changed.
  EXPECT_TRUE(std::is_copy_constructible<const HolderType>::value);
  EXPECT_FALSE(std::is_copy_assignable<const HolderType>::value);
  EXPECT_TRUE(std::is_move_constructible<const HolderType>::value);
  EXPECT_FALSE(std::is_move_assignable<const HolderType>::value);
  const HolderType const_holder(2);
  testutil::ExpectSameType<const int&, decltype(const_holder.value())>();
  EXPECT_EQ(2, const_holder.value());
}

TEST(Ref, NotRefCountable_cost) {
  // All modes work (unlike Copy).
  using HolderType = Holder<const int, Ref<Copy>>;
  EXPECT_TRUE(std::is_copy_constructible<HolderType>::value);
  EXPECT_TRUE(std::is_copy_assignable<HolderType>::value);
  EXPECT_TRUE(std::is_move_constructible<HolderType>::value);
  EXPECT_TRUE(std::is_move_assignable<HolderType>::value);

  // Value cannot be changed.
  HolderType holder(1);
  testutil::ExpectSameType<const int&, decltype(holder.value())>();
  EXPECT_EQ(1, holder.value());

  // Value is shared.
  HolderType holder2 = holder;
  EXPECT_EQ(&holder.value(), &holder2.value());

  // Const holder has the same properties.
  EXPECT_TRUE(std::is_copy_constructible<const HolderType>::value);
  EXPECT_FALSE(std::is_copy_assignable<const HolderType>::value);
  EXPECT_TRUE(std::is_move_constructible<const HolderType>::value);
  EXPECT_FALSE(std::is_move_assignable<const HolderType>::value);
  const HolderType const_holder(2);
  testutil::ExpectSameType<const int&, decltype(const_holder.value())>();
  EXPECT_EQ(2, const_holder.value());
}

TEST(RefPtr, RefCountable) {
  using HolderType = Holder<TestRefCounted, RefPtr>;
  EXPECT_TRUE(std::is_copy_constructible<HolderType>::value);
  EXPECT_TRUE(std::is_copy_assignable<HolderType>::value);
  EXPECT_TRUE(std::is_move_constructible<HolderType>::value);
  EXPECT_TRUE(std::is_move_assignable<HolderType>::value);

  // Value can be mutated.
  HolderType holder(new TestRefCounted(1));
  testutil::ExpectSameType<TestRefCounted&, decltype(holder.value())>();
  EXPECT_EQ(1, holder.value().id());
  holder.value() = TestRefCounted(2);
  EXPECT_EQ(2, holder.value().id());

  // Value is shared.
  HolderType holder2 = holder;
  EXPECT_EQ(&holder.value(), &holder2.value());

  // Const holder cannot be assigned or have its value changed.
  EXPECT_TRUE(std::is_copy_constructible<const HolderType>::value);
  EXPECT_FALSE(std::is_copy_assignable<const HolderType>::value);
  EXPECT_TRUE(std::is_move_constructible<const HolderType>::value);
  EXPECT_FALSE(std::is_move_assignable<const HolderType>::value);
  const HolderType const_holder(new TestRefCounted(2));
  testutil::ExpectSameType<const TestRefCounted&,
                           decltype(const_holder.value())>();
  EXPECT_EQ(2, const_holder.value().id());
}

TEST(RefPtr, RefCountable_cost) {
  // All modes work (unlike Copy).
  using HolderType = Holder<const TestRefCounted, RefPtr>;
  EXPECT_TRUE(std::is_copy_constructible<HolderType>::value);
  EXPECT_TRUE(std::is_copy_assignable<HolderType>::value);
  EXPECT_TRUE(std::is_move_constructible<HolderType>::value);
  EXPECT_TRUE(std::is_move_assignable<HolderType>::value);

  // Value cannot be changed.
  HolderType holder(new TestRefCounted(1));
  testutil::ExpectSameType<const TestRefCounted&, decltype(holder.value())>();
  EXPECT_EQ(1, holder.value().id());

  // Value is shared.
  HolderType holder2 = holder;
  EXPECT_EQ(&holder.value(), &holder2.value());

  // Const holder has the same properties.
  EXPECT_TRUE(std::is_copy_constructible<const HolderType>::value);
  EXPECT_FALSE(std::is_copy_assignable<const HolderType>::value);
  EXPECT_TRUE(std::is_move_constructible<const HolderType>::value);
  EXPECT_FALSE(std::is_move_assignable<const HolderType>::value);
  const HolderType const_holder(new TestRefCounted(2));
  testutil::ExpectSameType<const TestRefCounted&,
                           decltype(const_holder.value())>();
  EXPECT_EQ(2, const_holder.value().id());
}

TEST(SizeLimitHolder, Inline16) {
  using HolderType = SizeLimitHolder<int16_t, 8>;
  EXPECT_TRUE(std::is_copy_constructible<HolderType>::value);
  EXPECT_TRUE(std::is_copy_assignable<HolderType>::value);
  EXPECT_TRUE(std::is_move_constructible<HolderType>::value);
  EXPECT_TRUE(std::is_move_assignable<HolderType>::value);
  EXPECT_LE(sizeof(int16_t), 8);
  EXPECT_LE(sizeof(HolderType), 8);
  EXPECT_EQ(sizeof(HolderType), sizeof(int16_t));
  HolderType h1(1);
  HolderType h2 = h1;

  testutil::ExpectSameType<const int16_t&, decltype(h1.value())>();
  EXPECT_EQ(h1.value(), h2.value());
  EXPECT_NE(&h1.value(), &h2.value());
}

TEST(SizeLimitHolder, Inline64) {
  using HolderType = SizeLimitHolder<int64_t, 8>;
  EXPECT_TRUE(std::is_copy_constructible<HolderType>::value);
  EXPECT_TRUE(std::is_copy_assignable<HolderType>::value);
  EXPECT_TRUE(std::is_move_constructible<HolderType>::value);
  EXPECT_TRUE(std::is_move_assignable<HolderType>::value);
  EXPECT_LE(sizeof(int64_t), 8);
  EXPECT_LE(sizeof(HolderType), 8);
  EXPECT_EQ(sizeof(HolderType), sizeof(int64_t));

  HolderType h1(1);
  HolderType h2 = h1;

  testutil::ExpectSameType<const int64_t&, decltype(h1.value())>();
  EXPECT_EQ(h1.value(), h2.value());
  EXPECT_NE(&h1.value(), &h2.value());
}

TEST(SizeLimitHolder, OverSized) {
  using HolderType = SizeLimitHolder<TestRefCounted, 8>;
  EXPECT_TRUE(std::is_copy_constructible<HolderType>::value);
  EXPECT_TRUE(std::is_copy_assignable<HolderType>::value);
  EXPECT_TRUE(std::is_move_constructible<HolderType>::value);
  EXPECT_TRUE(std::is_move_assignable<HolderType>::value);
  EXPECT_GT(sizeof(TestRefCounted), 8);
  EXPECT_LE(sizeof(HolderType), 8);

  HolderType h1(TestRefCounted(1));
  HolderType h2 = h1;

  testutil::ExpectSameType<const TestRefCounted&, decltype(h1.value())>();
  EXPECT_EQ(h1.value().id(), h2.value().id());
  EXPECT_EQ(&h1.value(), &h2.value());
}

}  // namespace
}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google
