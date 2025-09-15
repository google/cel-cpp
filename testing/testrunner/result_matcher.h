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

#ifndef THIRD_PARTY_CEL_CPP_TESTING_TESTRUNNER_RESULT_MATCHER_H_
#define THIRD_PARTY_CEL_CPP_TESTING_TESTRUNNER_RESULT_MATCHER_H_

#include "common/value.h"
#include "cel/expr/conformance/test/suite.pb.h"
#include "google/protobuf/arena.h"

namespace cel::test {

// Forward declare CelTestContext to avoid circular includes.
class CelTestContext;

// Parameters passed to the ResultMatcher for performing assertions.
struct ResultMatcherParams {
  const cel::expr::conformance::test::TestOutput& expected_output;
  const CelTestContext& test_context;
  const cel::Value& computed_output;
  google::protobuf::Arena* arena;
};

// Interface for a custom result matcher.
class ResultMatcher {
 public:
  virtual ~ResultMatcher() = default;
  virtual void Match(const ResultMatcherParams& params) const = 0;
};

}  // namespace cel::test

#endif  // THIRD_PARTY_CEL_CPP_TESTING_TESTRUNNER_RESULT_MATCHER_H_
