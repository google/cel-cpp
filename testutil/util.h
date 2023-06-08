#ifndef THIRD_PARTY_CEL_CPP_TESTUTIL_EXPECT_SAME_TYPE_H_
#define THIRD_PARTY_CEL_CPP_TESTUTIL_EXPECT_SAME_TYPE_H_

#include <memory>
#include <ostream>
#include <string>

#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "gmock/gmock.h"
#include "absl/log/absl_check.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "internal/casts.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"

namespace google {
namespace api {
namespace expr {
namespace testutil {

// A helper class that causes the compiler to print a helpful error when
// they template args don't match.
template <typename E, typename A>
struct ExpectSameType;

template <typename T>
struct ExpectSameType<T, T> {};

/**
 * Simple implementation of a proto matcher comparing string representations.
 *
 * IMPORTANT: Only use this for protos whose textual representation is
 * deterministic (that may not be the case for the map collection type).
 */
class TextProtoMatcher {
 public:
  explicit inline TextProtoMatcher(absl::string_view expected)
      : expected_(expected) {}

  bool MatchAndExplain(const google::protobuf::MessageLite& p,
                       ::testing::MatchResultListener* listener) const {
    return MatchAndExplain(cel::internal::down_cast<const google::protobuf::Message&>(p),
                           listener);
  }

  bool MatchAndExplain(const google::protobuf::MessageLite* p,
                       ::testing::MatchResultListener* listener) const {
    return MatchAndExplain(cel::internal::down_cast<const google::protobuf::Message*>(p),
                           listener);
  }

  bool MatchAndExplain(const google::protobuf::Message& p,
                       ::testing::MatchResultListener* listener) const {
    auto message = absl::WrapUnique(p.New());
    ABSL_CHECK(google::protobuf::TextFormat::ParseFromString(expected_, message.get()));
    return google::protobuf::util::MessageDifferencer::Equals(
        *message, cel::internal::down_cast<const google::protobuf::Message&>(p));
  }

  bool MatchAndExplain(const google::protobuf::Message* p,
                       ::testing::MatchResultListener* listener) const {
    auto message = absl::WrapUnique(p->New());
    ABSL_CHECK(google::protobuf::TextFormat::ParseFromString(expected_, message.get()));
    return google::protobuf::util::MessageDifferencer::Equals(
        *message, cel::internal::down_cast<const google::protobuf::Message&>(*p));
  }

  inline void DescribeTo(::std::ostream* os) const { *os << expected_; }
  inline void DescribeNegationTo(::std::ostream* os) const {
    *os << "not equal to expected message: " << expected_;
  }

 private:
  const std::string expected_;
};

/**
 * Simple implementation of a proto matcher comparing string representations.
 *
 * IMPORTANT: Only use this for protos whose textual representation is
 * deterministic (that may not be the case for the map collection type).
 */
class ProtoMatcher {
 public:
  explicit inline ProtoMatcher(const google::protobuf::Message& expected)
      : expected_(expected.New()) {
    expected_->CopyFrom(expected);
  }

  bool MatchAndExplain(const google::protobuf::MessageLite& p,
                       ::testing::MatchResultListener* listener) const {
    return MatchAndExplain(cel::internal::down_cast<const google::protobuf::Message&>(p),
                           listener);
  }

  bool MatchAndExplain(const google::protobuf::MessageLite* p,
                       ::testing::MatchResultListener* listener) const {
    return MatchAndExplain(cel::internal::down_cast<const google::protobuf::Message*>(p),
                           listener);
  }

  bool MatchAndExplain(const google::protobuf::Message& p,
                       ::testing::MatchResultListener* /* listener */) const {
    return google::protobuf::util::MessageDifferencer::Equals(*expected_, p);
  }

  bool MatchAndExplain(const google::protobuf::Message* p,
                       ::testing::MatchResultListener* /* listener */) const {
    return google::protobuf::util::MessageDifferencer::Equals(*expected_, *p);
  }

  inline void DescribeTo(::std::ostream* os) const {
    *os << expected_->DebugString();
  }
  inline void DescribeNegationTo(::std::ostream* os) const {
    *os << "not equal to expected message: " << expected_->DebugString();
  }

 private:
  std::shared_ptr<google::protobuf::Message> expected_;
};

// Polymorphic matcher to compare any two protos.
inline ::testing::PolymorphicMatcher<TextProtoMatcher> EqualsProto(
    absl::string_view x) {
  return ::testing::MakePolymorphicMatcher(TextProtoMatcher(x));
}

// Polymorphic matcher to compare any two protos.
inline ::testing::PolymorphicMatcher<ProtoMatcher> EqualsProto(
    const google::protobuf::Message& x) {
  return ::testing::MakePolymorphicMatcher(ProtoMatcher(x));
}

}  // namespace testutil
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_TESTUTIL_EXPECT_SAME_TYPE_H_
