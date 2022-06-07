#ifndef THIRD_PARTY_CEL_CPP_TESTUTIL_EXPECT_SAME_TYPE_H_
#define THIRD_PARTY_CEL_CPP_TESTUTIL_EXPECT_SAME_TYPE_H_

#include <string>

#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "gmock/gmock.h"
#include "absl/strings/string_view.h"

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

// Creates a proto message of type T from a textual representation.
template <typename T>
T CreateProto(const std::string& textual_proto);

/**
 * Simple implementation of a proto matcher comparing string representations.
 *
 * IMPORTANT: Only use this for protos whose textual representation is
 * deterministic (that may not be the case for the map collection type).
 */
class ProtoStringMatcher {
 public:
  explicit inline ProtoStringMatcher(absl::string_view expected)
      : expected_(expected) {}

  explicit inline ProtoStringMatcher(const google::protobuf::Message& expected)
      : expected_(expected.DebugString()) {}

  template <typename Message>
  bool MatchAndExplain(const Message& p,
                       ::testing::MatchResultListener* /* listener */) const;

  template <typename Message>
  bool MatchAndExplain(const Message* p,
                       ::testing::MatchResultListener* /* listener */) const;

  inline void DescribeTo(::std::ostream* os) const { *os << expected_; }
  inline void DescribeNegationTo(::std::ostream* os) const {
    *os << "not equal to expected message: " << expected_;
  }

 private:
  const std::string expected_;
};

// Polymorphic matcher to compare any two protos.
inline ::testing::PolymorphicMatcher<ProtoStringMatcher> EqualsProto(
    absl::string_view x) {
  return ::testing::MakePolymorphicMatcher(ProtoStringMatcher(x));
}

// Polymorphic matcher to compare any two protos.
inline ::testing::PolymorphicMatcher<ProtoStringMatcher> EqualsProto(
    const google::protobuf::Message& x) {
  return ::testing::MakePolymorphicMatcher(ProtoStringMatcher(x));
}

template <typename T>
T CreateProto(const std::string& textual_proto) {
  T proto;
  google::protobuf::TextFormat::ParseFromString(textual_proto, &proto);
  return proto;
}

template <typename Message>
bool ProtoStringMatcher::MatchAndExplain(
    const Message& p, ::testing::MatchResultListener* /* listener */) const {
  // Need to CreateProto and then print as std::string so that the formatting
  // matches exactly.
  return p.SerializeAsString() ==
         CreateProto<Message>(expected_).SerializeAsString();
}

template <typename Message>
bool ProtoStringMatcher::MatchAndExplain(
    const Message* p, ::testing::MatchResultListener* /* listener */) const {
  // Need to CreateProto and then print as std::string so that the formatting
  // matches exactly.
  std::unique_ptr<google::protobuf::Message> value;
  value.reset(p->New());
  google::protobuf::TextFormat::ParseFromString(expected_, value.get());
  return p->SerializeAsString() == value->SerializeAsString();
}

}  // namespace testutil
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_TESTUTIL_EXPECT_SAME_TYPE_H_
