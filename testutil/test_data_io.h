#ifndef THIRD_PARTY_CEL_CPP_TESTUTIL_TEST_DATA_IO_H_
#define THIRD_PARTY_CEL_CPP_TESTUTIL_TEST_DATA_IO_H_

#include "google/rpc/status.pb.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "testdata/test_data.pb.h"
#include "testdata/test_value.pb.h"

namespace google {
namespace api {
namespace expr {
namespace testutil {

/** The file extension for a text proto file. */
constexpr const char kTextPbExt[] = ".textpb";
/** The file extension for a binary proto file. */
constexpr const char kBinaryPbExt[] = ".binarypb";

/**
 * Reads the given proto file.
 *
 * The file is treated as text if it ends with `kTextPbExt`, and binary
 * in all other cases.
 */
google::rpc::Status ReadPbFile(absl::string_view absolute_file_path,
                               google::protobuf::Message* message);

/**
 * Writes the given proto file.
 *
 * The file is written as text if it ends with `kTextPbExt`, and binary
 * in all other cases.
 */
google::rpc::Status WritePbFile(const google::protobuf::Message& message,
                                absl::string_view absolute_file_path);

/**
 * Writes test data under the given test_name.
 *
 * Output is controlled by the following flags:
 *   - binary: If the output should be in a proto binary format.
 *   - test_data_folder: The folder to write the files.
 */
google::rpc::Status WriteTestData(absl::string_view test_name,
                                  const testdata::TestData& values);

/**
 * Reads test data for the given test.
 *
 * Read from the dir specified by the `test_data_folder` flag and CHECK fails
 * if the test cannot be found.
 */
testdata::TestData ReadTestData(absl::string_view test_name);

/**
 * A helper class to generate friendly test names.
 */
struct TestDataParamName {
  std::string operator()(
      const ::testing::TestParamInfo<testdata::TestValue>& info) const;
  std::string operator()(
      const ::testing::TestParamInfo<
          std::pair<testdata::TestValue, testdata::TestValue>>& info) const;
};

}  // namespace testutil
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_TESTUTIL_TEST_DATA_IO_H_
