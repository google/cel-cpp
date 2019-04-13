#include "eval/public/activation.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using testing::Eq;
using testing::Return;
using ::google::protobuf::Arena;

class MockValueProducer : public CelValueProducer {
 public:
  MOCK_METHOD1(Produce, CelValue(Arena*));
};

TEST(ActivationTest, CheckValueInsertFindAndRemove) {
  Activation activation;

  Arena arena;

  activation.InsertValue("value42", CelValue::CreateInt64(42));

  // Test getting unbound value
  EXPECT_FALSE(activation.FindValue("value43", &arena));

  // Test getting bound value
  EXPECT_TRUE(activation.FindValue("value42", &arena));

  CelValue value = activation.FindValue("value42", &arena).value();

  // Test value is correct.
  EXPECT_THAT(value.Int64OrDie(), Eq(42));

  // Test removing unbound value
  EXPECT_FALSE(activation.RemoveValueEntry("value43"));

  // Test removing bound value
  EXPECT_TRUE(activation.RemoveValueEntry("value42"));

  // Now the value is unbound
  EXPECT_FALSE(activation.FindValue("value42", &arena));
}

TEST(ActivationTest, CheckValueProducerInsertFindAndRemove) {
  const std::string kValue = "42";

  auto producer = absl::make_unique<MockValueProducer>();

  google::protobuf::Arena arena;

  ON_CALL(*producer, Produce(&arena))
      .WillByDefault(Return(CelValue::CreateString(&kValue)));

  // ValueProducer is expected to be invoked only once.
  EXPECT_CALL(*producer, Produce(&arena)).Times(1);

  Activation activation;

  activation.InsertValueProducer("value42", std::move(producer));

  // Test getting unbound value
  EXPECT_FALSE(activation.FindValue("value43", &arena));

  // Test getting bound value - 1st pass

  // Access attempt is repeated twice.
  // ValueProducer is expected to be invoked only once.
  for (int i = 0; i < 2; i++) {
    auto opt_value = activation.FindValue("value42", &arena);
    EXPECT_TRUE(opt_value.has_value()) << " for pass " << i;
    CelValue value = opt_value.value();
    EXPECT_THAT(value.StringOrDie().value(), Eq(kValue)) << " for pass " << i;
  }

  // Test removing bound value
  EXPECT_TRUE(activation.RemoveValueEntry("value42"));

  // Now the value is unbound
  EXPECT_FALSE(activation.FindValue("value42", &arena));
}

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
