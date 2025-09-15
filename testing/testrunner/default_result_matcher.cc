// Copyright 2025 Google LLC
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

#include <memory>

#include "cel/expr/eval.pb.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "common/internal/value_conversion.h"
#include "common/value.h"
#include "internal/testing.h"
#include "testing/testrunner/cel_test_context.h"
#include "testing/testrunner/result_matcher.h"
#include "cel/expr/conformance/test/suite.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/util/field_comparator.h"
#include "google/protobuf/util/message_differencer.h"

namespace cel::test {

namespace {

using ValueProto = ::cel::expr::Value;
using ::cel::expr::conformance::test::TestOutput;

bool IsEqual(const ValueProto& expected, const ValueProto& actual) {
  static auto* kFieldComparator = []() {
    auto* field_comparator = new google::protobuf::util::DefaultFieldComparator();
    field_comparator->set_treat_nan_as_equal(true);
    return field_comparator;
  }();
  static auto* kDifferencer = []() {
    auto* differencer = new google::protobuf::util::MessageDifferencer();
    differencer->set_message_field_comparison(
        google::protobuf::util::MessageDifferencer::EQUIVALENT);
    differencer->set_field_comparator(kFieldComparator);
    const auto* descriptor = cel::expr::MapValue::descriptor();
    const auto* entries_field = descriptor->FindFieldByName("entries");
    const auto* key_field =
        entries_field->message_type()->FindFieldByName("key");
    differencer->TreatAsMap(entries_field, key_field);
    return differencer;
  }();
  return kDifferencer->Compare(expected, actual);
}

MATCHER_P(MatchesValue, expected, "") { return IsEqual(arg, expected); }

class DefaultResultMatcher : public cel::test::ResultMatcher {
 public:
  void Match(const ResultMatcherParams& params) const override {
    const TestOutput& output = params.expected_output;
    const auto& computed_output = params.computed_output;
    google::protobuf::Arena* arena = params.arena;

    if (output.has_result_value()) {
      AssertValue(computed_output, output, params.test_context, arena);
    } else if (output.has_eval_error()) {
      AssertError(computed_output, output);
    } else if (output.has_unknown()) {
      ADD_FAILURE() << "Unknown assertions not implemented yet.";
    } else {
      ADD_FAILURE() << "Unexpected output kind.";
    }
  }

 private:
  void AssertValue(const cel::Value& computed, const TestOutput& output,
                   const CelTestContext& test_context,
                   google::protobuf::Arena* arena) const {
    ValueProto expected_value_proto;
    const auto* descriptor_pool =
        test_context.runtime() != nullptr
            ? test_context.runtime()->GetDescriptorPool()
            : google::protobuf::DescriptorPool::generated_pool();
    auto* message_factory = test_context.runtime() != nullptr
                                ? test_context.runtime()->GetMessageFactory()
                                : google::protobuf::MessageFactory::generated_factory();

    ValueProto computed_expr_value;
    ASSERT_OK_AND_ASSIGN(
        computed_expr_value,
        ToExprValue(computed, descriptor_pool, message_factory, arena));
    EXPECT_THAT(output.result_value(), MatchesValue(computed_expr_value));
  }

  void AssertError(const cel::Value& computed, const TestOutput& output) const {
    if (!computed.IsError()) {
      ADD_FAILURE() << "Expected error but got value: "
                    << computed.DebugString();
      return;
    }
    absl::Status computed_status = computed.AsError()->ToStatus();
    // We selected the first error in the set for comparison because there is
    // only one runtime error that is reported even if there are multiple errors
    // in the critical path.
    ASSERT_TRUE(output.eval_error().errors_size() == 1)
        << "Expected exactly one error but got: "
        << output.eval_error().errors_size();
    ASSERT_EQ(computed_status.message(),
              output.eval_error().errors(0).message());
  }
};
}  // namespace

std::unique_ptr<ResultMatcher> CreateDefaultResultMatcher() {
  return std::make_unique<DefaultResultMatcher>();
}
}  // namespace cel::test
