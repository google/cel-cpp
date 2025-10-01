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
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <ios>
#include <iostream>
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
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "eval/public/cel_expression.h"
#include "internal/testing.h"
#include "runtime/runtime.h"
#include "testing/testrunner/cel_test_context.h"
#include "testing/testrunner/cel_test_factories.h"
#include "testing/testrunner/coverage_index.h"
#include "testing/testrunner/runner_lib.h"
#include "cel/expr/conformance/test/suite.pb.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/text_format.h"

ABSL_FLAG(std::string, test_suite_path, "",
          "The path to the file containing the test suite to run.");

ABSL_FLAG(bool, collect_coverage, false, "Whether to collect code coverage.");

namespace {

using ::cel::expr::conformance::test::TestCase;
using ::cel::expr::conformance::test::TestSuite;
using ::cel::test::CelTestContext;
using ::cel::test::CoverageIndex;
using ::cel::test::TestRunner;
using ::google::api::expr::runtime::CelExpressionBuilder;

class CoverageReportingEnvironment : public testing::Environment {
 public:
  explicit CoverageReportingEnvironment(CoverageIndex& coverage_index)
      : coverage_index_(coverage_index) {}

  void TearDown() override {
    CoverageIndex::CoverageReport coverage_report =
        coverage_index_.GetCoverageReport();
    testing::Test::RecordProperty("CEL Expression",
                                  coverage_report.cel_expression);
    std::cout << "CEL Expression: " << coverage_report.cel_expression;

    if (coverage_report.nodes == 0) {
      testing::Test::RecordProperty("CEL Coverage", "No coverage stats found");
      std::cout << "CEL Coverage: " << "No coverage stats found";
      return;
    }

    // Log Node Coverage results
    double node_coverage = static_cast<double>(coverage_report.covered_nodes) /
                           static_cast<double>(coverage_report.nodes) * 100.0;
    std::string node_coverage_string =
        absl::StrFormat("%.2f%% (%d out of %d nodes covered)", node_coverage,
                        coverage_report.covered_nodes, coverage_report.nodes);
    testing::Test::RecordProperty("AST Node Coverage", node_coverage_string);
    std::cout << "AST Node Coverage: " << node_coverage_string;
    if (!coverage_report.unencountered_nodes.empty()) {
      testing::Test::RecordProperty(
          "Interesting Unencountered Nodes",
          absl::StrJoin(coverage_report.unencountered_nodes, "\n"));
      std::cout << "Interesting Unencountered Nodes: "
                << absl::StrJoin(coverage_report.unencountered_nodes, "\n");
    }
    // Log Branch Coverage results
    double branch_coverage = 0.0;
    if (coverage_report.branches > 0) {
      branch_coverage =
          static_cast<double>(coverage_report.covered_boolean_outcomes) /
          static_cast<double>(coverage_report.branches) * 100.0;
    }
    std::string branch_coverage_string = absl::StrFormat(
        "%.2f%% (%d out of %d branch outcomes covered)", branch_coverage,
        coverage_report.covered_boolean_outcomes, coverage_report.branches);
    testing::Test::RecordProperty("AST Branch Coverage",
                                  branch_coverage_string);
    std::cout << "AST Branch Coverage: " << branch_coverage_string;
    if (!coverage_report.unencountered_branches.empty()) {
      testing::Test::RecordProperty(
          "Interesting Unencountered Branch Paths",
          absl::StrJoin(coverage_report.unencountered_branches, "\n"));
      std::cout << "Interesting Unencountered Branch Paths: "
                << absl::StrJoin(coverage_report.unencountered_branches,
                                 "\n");
    }

    if (!coverage_report.dot_graph.empty()) {
      WriteDotGraphToArtifact(coverage_report.dot_graph);
    }
  }

 private:
  void WriteDotGraphToArtifact(absl::string_view dot_graph) {
    // Save DOT graph to file in TEST_UNDECLARED_OUTPUTS_DIR or default dir
    const char* outputs_dir_env = std::getenv("TEST_UNDECLARED_OUTPUTS_DIR");

    // For non-Bazel/Blaze users, we write to a subdirectory under the current
    // working directory.
    // NOMUTANTS --cel_artifacts is for non-Bazel/Blaze users only so not
    // needed to test in our case.
    std::string outputs_dir =
        (outputs_dir_env == nullptr) ? "cel_artifacts" : outputs_dir_env;

    std::string coverage_dir = absl::StrCat(outputs_dir, "/cel_test_coverage");

    // Creates the directory to store CEL test coverage artifacts.
    // The second argument, `0755`, sets the directory's permissions in octal
    // format, which is a standard for file system operations. It grants:
    //   - Owner: read, write, and execute permissions (7 = 4+2+1).
    //   - Group: read and execute permissions (5 = 4+1).
    //   - Others: read and execute permissions (5 = 4+1).
    // This gives the owner full control while allowing other users to access
    // the generated artifacts.
    int mkdir_result = mkdir(coverage_dir.c_str(), 0755);

    // If mkdir fails, it sets the global 'errno' variable to an error code
    // indicating the reason. We check this code to specifically ignore the
    // EEXIST error, which just means the directory already exists (this is not
    // a real failure we need to warn about).
    if (mkdir_result == 0 || errno == EEXIST) {
      std::string graph_path =
          absl::StrCat(coverage_dir, "/coverage_graph.txt");
      std::ofstream out(graph_path);
      if (out.is_open()) {
        out << dot_graph;
        out.close();
      } else {
        ABSL_LOG(WARNING) << "Failed to open file for writing: " << graph_path;
      }
    } else {
      ABSL_LOG(WARNING) << "Failed to create directory: " << coverage_dir
                        << " (reason: " << strerror(errno) << ")";
    }
  }

  cel::test::CoverageIndex& coverage_index_;
};

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
  absl::StatusOr<std::unique_ptr<CelTestContext>> cel_test_context =
      cel::test::internal::GetCelTestContextFactory()();
  if (!cel_test_context.ok()) {
    ABSL_LOG(FATAL) << "Failed to create CEL test context: "
                    << cel_test_context.status();
  }

  cel::test::CoverageIndex coverage_index;
  if (absl::GetFlag(FLAGS_collect_coverage)) {
    if (cel_test_context.value()->runtime() != nullptr) {
      ABSL_CHECK_OK(cel::test::EnableCoverageInRuntime(
          const_cast<cel::Runtime&>(*cel_test_context.value()->runtime()),
          coverage_index));
    } else if (cel_test_context.value()->cel_expression_builder() != nullptr) {
      ABSL_CHECK_OK(cel::test::EnableCoverageInCelExpressionBuilder(
          const_cast<CelExpressionBuilder&>(
              *cel_test_context.value()->cel_expression_builder()),
          coverage_index));
    }
  }

  auto test_runner =
      std::make_shared<TestRunner>(std::move(cel_test_context.value()));
  ABSL_CHECK_OK(RegisterTests(GetTestSuite(), test_runner));

  // Make sure the checked expression exists during the entire test run since
  // the ast references it during coverage collection at teardown.
  absl::StatusOr<cel::expr::CheckedExpr> checked_expr =
      test_runner->GetCheckedExpr();
  if (!checked_expr.ok()) {
    ABSL_LOG(FATAL) << "Failed to get checked expression: "
                    << checked_expr.status();
  }

  if (absl::GetFlag(FLAGS_collect_coverage)) {
    coverage_index.Init(*checked_expr);
    testing::AddGlobalTestEnvironment(
        new CoverageReportingEnvironment(coverage_index));
  }

  return RUN_ALL_TESTS();
}
