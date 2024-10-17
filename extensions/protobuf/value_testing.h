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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_VALUE_TESTING_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_VALUE_TESTING_H_

#include <ostream>
#include <type_traits>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "base/internal/message_wrapper.h"
#include "common/type.h"
#include "common/value.h"
#include "internal/testing.h"
#include "google/protobuf/message.h"

namespace cel::extensions::test {

template <typename MessageType>
class StructValueAsProtoMatcher {
 public:
  using is_gtest_matcher = void;

  explicit StructValueAsProtoMatcher(testing::Matcher<MessageType>&& m)
      : m_(std::move(m)) {}

  bool MatchAndExplain(cel::Value v,
                       testing::MatchResultListener* result_listener) const {
    MessageType msg;
    absl::Status s = ValueToMessage(v, &msg);
    if (!s.ok()) {
      *result_listener << "cannot convert to "
                       << MessageType::descriptor()->full_name() << ": " << s;
      return false;
    }
    return m_.MatchAndExplain(msg, result_listener);
  }

  void DescribeTo(std::ostream* os) const {
    *os << "matches proto message " << m_;
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "does not match proto message " << m_;
  }

 private:
  static absl::Status ValueToMessage(
      const cel::Value& value, absl::Nonnull<google::protobuf::Message*> dest_message) {
    const auto* dest_descriptor = dest_message->GetDescriptor();
    if (auto legacy_struct_value =
            cel::common_internal::AsLegacyStructValue(value);
        legacy_struct_value) {
      const auto* src_message = reinterpret_cast<const google::protobuf::Message*>(
          legacy_struct_value->message_ptr() &
          cel::base_internal::kMessageWrapperPtrMask);
      return ValueToMessage(*src_message, dest_message);
    }
    if (auto parsed_message_value = value.AsParsedMessage();
        parsed_message_value) {
      return ValueToMessage(**parsed_message_value, dest_message);
    }
    return TypeConversionError(value.GetRuntimeType(),
                               cel::MessageType(dest_descriptor))
        .NativeValue();
  }

  static absl::Status ValueToMessage(
      const google::protobuf::Message& src_message,
      absl::Nonnull<google::protobuf::Message*> dest_message) {
    const auto* src_descriptor = src_message.GetDescriptor();
    const auto* dest_descriptor = dest_message->GetDescriptor();
    if (dest_descriptor == src_descriptor ||
        dest_descriptor->full_name() == src_descriptor->full_name()) {
      absl::Cord serialized;
      if (!src_message.SerializePartialToCord(&serialized)) {
        return absl::UnknownError(absl::StrCat("failed to serialize message: ",
                                               src_descriptor->full_name()));
      }
      if (!dest_message->ParsePartialFromCord(serialized)) {
        return absl::UnknownError(absl::StrCat("failed to parse message: ",
                                               dest_descriptor->full_name()));
      }
      return absl::OkStatus();
    }
    return TypeConversionError(cel::MessageType(src_descriptor),
                               cel::MessageType(dest_descriptor))
        .NativeValue();
  }

  testing::Matcher<MessageType> m_;
};

// Returns a matcher that matches a cel::Value against a proto message.
//
// Example usage:
//
//   EXPECT_THAT(value, StructValueAsProto<TestAllTypes>(EqualsProto(R"pb(
//                 single_int32: 1
//                 single_string: "foo"
//               )pb")));
template <typename MessageType>
inline StructValueAsProtoMatcher<MessageType> StructValueAsProto(
    testing::Matcher<MessageType>&& m) {
  static_assert(std::is_base_of_v<google::protobuf::Message, MessageType>);
  return StructValueAsProtoMatcher<MessageType>(std::move(m));
}

}  // namespace cel::extensions::test

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_VALUE_TESTING_H_
