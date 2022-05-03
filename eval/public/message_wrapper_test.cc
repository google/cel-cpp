// Copyright 2022 Google LLC
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

#include "eval/public/message_wrapper.h"

#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"
#include "eval/public/structs/trivial_legacy_type_info.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {
namespace {

TEST(MessageWrapper, Size) {
  static_assert(sizeof(MessageWrapper) <= 2 * sizeof(uintptr_t),
                "MessageWrapper must not increase CelValue size.");
}

TEST(MessageWrapper, WrapsMessage) {
  TestMessage test_message;

  test_message.set_int64_value(20);
  test_message.set_double_value(12.3);

  MessageWrapper wrapped_message(&test_message, TrivialTypeInfo::GetInstance());

  constexpr bool is_full_proto_runtime =
      std::is_base_of_v<google::protobuf::Message, TestMessage>;

  EXPECT_EQ(wrapped_message.message_ptr(),
            static_cast<const google::protobuf::MessageLite*>(&test_message));
  ASSERT_EQ(wrapped_message.HasFullProto(), is_full_proto_runtime);
}

TEST(MessageWrapper, DefaultNull) {
  MessageWrapper wrapper;
  EXPECT_EQ(wrapper.message_ptr(), nullptr);
  EXPECT_EQ(wrapper.legacy_type_info(), nullptr);
}

}  // namespace
}  // namespace google::api::expr::runtime
