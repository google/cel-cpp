// Copyright 2025 Google LLC
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

// This binary is a test runner for CEL tests. It is used to run CEL tests
// written in the CEL test suite format.
#include <fstream>
#include <functional>
#include <ios>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "internal/testing.h"
#include "testing/testrunner/cel_test_context.h"
#include "testing/testrunner/cel_test_factories.h"
#include "testing/testrunner/runner_lib.h"
#include "cel/expr/conformance/test/suite.pb.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/text_format.h"

ABSL_FLAG(std::string, test_suite_path, "",
          "The path to the file containing the test suite to run.");

namespace {

using ::cel::expr::conformance::test::TestCase;
using ::cel::expr::conformance::test::TestSuite;
using ::cel::test::TestRunner;

class CelTest : public testing::Test {
 public:
  explicit CelTest(std::shared_ptr<TestRunner> test_runner,
                   const TestCase& test_case)
      : test_runner_(std::move(test_runner)), test_case_(test_case) {}

  void TestBody() override { test_runner_->RunTest(test_case_); }

 private:
  std::shared_ptr<TestRunner> test_runner_;
  TestCase test_case_;
};

absl::Status RegisterTests(const TestSuite& test_suite,
                           const std::shared_ptr<TestRunner>& test_runner) {
  for (const auto& section : test_suite.sections()) {
    for (const TestCase& test_case : section.tests()) {
      testing::RegisterTest(
          test_suite.name().c_str(),
          absl::StrCat(section.name(), "/", test_case.name()).c_str(), nullptr,
          nullptr, __FILE__, __LINE__, [&test_runner, test_case]() -> CelTest* {
            return new CelTest(test_runner, test_case);
          });
    }
  }
  return absl::OkStatus();
}

TestSuite ReadTestSuiteFromPath(std::string_view test_suite_path) {
  TestSuite test_suite;
  {
    std::ifstream in;
    in.open(std::string(test_suite_path),
            std::ios_base::in | std::ios_base::binary);
    if (!in.is_open()) {
      ABSL_LOG(FATAL) << "failed to open file: " << test_suite_path;
    }
    google::protobuf::io::IstreamInputStream stream(&in);
    if (!google::protobuf::TextFormat::Parse(&stream, &test_suite)) {
      ABSL_LOG(FATAL) << "failed to parse file: " << test_suite_path;
    }
  }
  return test_suite;
}

TestSuite GetTestSuite() {
  std::string test_suite_path = absl::GetFlag(FLAGS_test_suite_path);
  if (!test_suite_path.empty()) {
    return ReadTestSuiteFromPath(test_suite_path);
  }

  // If no test suite path is provided, use the factory function to get the
  // test suite after checking if the factory function is empty or not.
  std::function<TestSuite()> test_suite_factory =
      cel::test::internal::GetCelTestSuiteFactory();
  if (test_suite_factory == nullptr) {
    ABSL_LOG(FATAL)
        << "No CEL test suite provided. Please provide a test suite using "
           "either the bzl macro or the CEL_REGISTER_TEST_SUITE_FACTORY "
           "preprocessor macro.";
  }
  return test_suite_factory();
}
}  // namespace

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);

  // Create a test context using the factory function returned by the global
  // factory function provider which was initialized by the user.
  absl::StatusOr<std::unique_ptr<cel::test::CelTestContext>> cel_test_context =
      cel::test::internal::GetCelTestContextFactory()();
  if (!cel_test_context.ok()) {
    ABSL_LOG(FATAL) << "Failed to create CEL test context: "
                    << cel_test_context.status();
  }
  auto test_runner =
      std::make_shared<TestRunner>(std::move(cel_test_context.value()));
  ABSL_CHECK_OK(RegisterTests(GetTestSuite(), test_runner));

  return RUN_ALL_TESTS();
}
