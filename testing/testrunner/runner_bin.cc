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
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "cel/expr/checked.pb.h"
#include "absl/flags/flag.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "testing/testrunner/cel_expression_source.h"
#include "testing/testrunner/cel_test_context.h"
#include "testing/testrunner/cel_test_factories.h"
#include "testing/testrunner/runner_lib.h"
#include "cel/expr/conformance/test/suite.pb.h"
#include "google/protobuf/text_format.h"

ABSL_FLAG(std::string, test_suite_path, "",
          "The path to the file containing the test suite to run.");
ABSL_FLAG(std::string, expression_kind, "",
          "The kind of expression source: 'raw', 'file', or 'checked'.");
ABSL_FLAG(std::string, cel_expr_value, "",
          "The value of the CEL expression source. For 'raw', it's the "
          "expression string. For 'file' and 'checked', it's the file path.");

namespace {

using ::cel::expr::conformance::test::TestCase;
using ::cel::expr::conformance::test::TestSuite;
using ::cel::test::CelExpressionSource;
using ::cel::test::CelTestContextOptions;
using ::cel::test::TestRunner;
using ::cel::expr::CheckedExpr;

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

absl::StatusOr<std::string> ReadFileToString(absl::string_view file_path) {
  std::ifstream file_stream{std::string(file_path)};
  if (!file_stream.is_open()) {
    return absl::NotFoundError(
        absl::StrCat("Unable to open file: ", file_path));
  }
  std::stringstream buffer;
  buffer << file_stream.rdbuf();
  return buffer.str();
}

template <typename T>
absl::StatusOr<T> ReadTextProtoFromFile(absl::string_view file_path) {
  CEL_ASSIGN_OR_RETURN(std::string contents, ReadFileToString(file_path));
  T message;
  if (!google::protobuf::TextFormat::ParseFromString(contents, &message)) {
    return absl::InternalError(absl::StrCat(
        "Failed to parse text-format proto from file: ", file_path));
  }
  return message;
}

TestSuite ReadTestSuiteFromPath(absl::string_view test_suite_path) {
  absl::StatusOr<TestSuite> test_suite_or =
      ReadTextProtoFromFile<TestSuite>(test_suite_path);

  if (!test_suite_or.ok()) {
    ABSL_LOG(FATAL) << "Failed to load test suite from " << test_suite_path
                    << ": " << test_suite_or.status();
  }
  return *std::move(test_suite_or);
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

absl::StatusOr<CelTestContextOptions> GetExpressionSourceOptions() {
  CelTestContextOptions options;
  if (absl::GetFlag(FLAGS_expression_kind).empty()) {
    return options;
  }

  std::string kind = absl::GetFlag(FLAGS_expression_kind);
  std::string value = absl::GetFlag(FLAGS_cel_expr_value);

  if (kind == "raw") {
    options.expression_source = CelExpressionSource::FromRawExpression(value);
  } else if (kind == "file") {
    options.expression_source = CelExpressionSource::FromCelFile(value);
  } else if (kind == "checked") {
    CEL_ASSIGN_OR_RETURN(CheckedExpr checked_expr,
                         ReadTextProtoFromFile<CheckedExpr>(value));
    options.expression_source =
        CelExpressionSource::FromCheckedExpr(std::move(checked_expr));
  } else {
    ABSL_LOG(FATAL) << "Unknown expression kind: " << kind;
  }
  return options;
}

}  // namespace

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  absl::StatusOr<CelTestContextOptions> test_context_options =
      GetExpressionSourceOptions();
  if (!test_context_options.ok()) {
    ABSL_LOG(FATAL) << "Failed to get expression source options: "
                    << test_context_options.status();
  }
  // Create a test context using the factory function returned by the global
  // factory function provider which was initialized by the user.
  absl::StatusOr<std::unique_ptr<cel::test::CelTestContext>> cel_test_context =
      cel::test::internal::GetCelTestContextFactory()(
          std::move(*test_context_options));
  if (!cel_test_context.ok()) {
    ABSL_LOG(FATAL) << "Failed to create CEL test context: "
                    << cel_test_context.status();
  }
  auto test_runner =
      std::make_shared<TestRunner>(std::move(cel_test_context.value()));
  ABSL_CHECK_OK(RegisterTests(GetTestSuite(), test_runner));

  return RUN_ALL_TESTS();
}
