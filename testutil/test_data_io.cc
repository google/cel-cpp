#include "testutil/test_data_io.h"

#include <fcntl.h>

#include <fstream>

#include "google/rpc/code.pb.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "absl/flags/flag.h"
#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "internal/status_util.h"
#include "re2/re2.h"

ABSL_FLAG(std::string, test_data_folder,
          "com_google_cel_spec/testdata/",
          "The location to read test data from.");

ABSL_FLAG(std::string, output_dir, "",
          "The location to write test data to. Writes to standard out if not "
          "specified.");

ABSL_FLAG(bool, binary, false, "If binary output should be used.");

namespace google {
namespace api {
namespace expr {
namespace testutil {

using testdata::TestData;
using testdata::TestValue;

namespace {

std::unique_ptr<google::protobuf::io::FileInputStream> OpenForRead(
    const std::string& filename) {
  int file_descriptor;
  do {
    file_descriptor = open(filename.c_str(), O_RDONLY);
  } while (file_descriptor < 0 && errno == EINTR);
  if (file_descriptor >= 0) {
    auto result =
        absl::make_unique<google::protobuf::io::FileInputStream>(file_descriptor);
    result->SetCloseOnDelete(true);
    return result;
  } else {
    return nullptr;
  }
}

std::unique_ptr<google::protobuf::io::FileOutputStream> OpenForWrite(
    const std::string& filename) {
  int file_descriptor;
  do {
    file_descriptor = open(filename.c_str(), O_WRONLY | O_CREAT);
  } while (file_descriptor < 0 && errno == EINTR);
  if (file_descriptor >= 0) {
    auto result =
        absl::make_unique<google::protobuf::io::FileOutputStream>(file_descriptor);
    result->SetCloseOnDelete(true);
    return result;
  } else {
    std::cerr << "Could not open file: " << errno << std::endl;
    return nullptr;
  }
}
std::string GetTestCaseFileName(absl::string_view dir,
                                absl::string_view test_name, bool binary) {
  return absl::StrCat(dir, test_name, binary ? kBinaryPbExt : kTextPbExt);
}

}  // namespace

google::rpc::Status ReadPbFile(absl::string_view absolute_file_path,
                               google::protobuf::Message* message) {
  message->Clear();
  auto in_stream = OpenForRead(std::string(absolute_file_path));
  if (in_stream == nullptr) {
    return internal::NotFoundError(
        absl::StrCat("File not found: ", absolute_file_path));
  }
  if (absl::EndsWith(absolute_file_path, kTextPbExt)) {
    if (google::protobuf::TextFormat::Parse(in_stream.get(), message)) {
      return internal::OkStatus();
    }
  } else {
    if (message->ParseFromZeroCopyStream(in_stream.get())) {
      return internal::OkStatus();
    }
  }
  return internal::InvalidArgumentError(
      absl::StrCat("Parsing file contents failed: ", absolute_file_path));
}

google::rpc::Status WritePbFile(const google::protobuf::Message& message,
                                absl::string_view absolute_file_path) {
  auto out_stream = OpenForWrite(std::string(absolute_file_path));
  if (out_stream == nullptr) {
    return internal::InvalidArgumentError(
        absl::StrCat("Could not open file: ", absolute_file_path));
  }
  if (absl::EndsWith(absolute_file_path, kTextPbExt)) {
    if (!google::protobuf::TextFormat::Print(message, out_stream.get())) {
      return internal::InvalidArgumentError(
          absl::StrCat("Unable to write to file: ", absolute_file_path));
    }
  } else {
    if (!message.SerializePartialToZeroCopyStream(out_stream.get())) {
      return internal::InvalidArgumentError(
          absl::StrCat("Unable to write to file: ", absolute_file_path));
    }
  }
  std::cout << "Wrote: " << absolute_file_path << std::endl;
  return internal::OkStatus();
}

google::rpc::Status WriteTestData(absl::string_view test_name,
                                  const TestData& values) {
  if (absl::GetFlag(FLAGS_output_dir).empty()) {
    google::protobuf::io::OstreamOutputStream os(&std::cout);
    google::protobuf::TextFormat::Print(values, &os);
    return internal::OkStatus();
  }
  return WritePbFile(
      values, GetTestCaseFileName(absl::GetFlag(FLAGS_output_dir), test_name,
                                  absl::GetFlag(FLAGS_binary)));
}

TestData ReadTestData(absl::string_view test_name) {
  TestData data;
  auto dir = absl::StrCat(std::getenv("TEST_SRCDIR"), "/",
                          absl::GetFlag(FLAGS_test_data_folder));

  auto status = ReadPbFile(
      GetTestCaseFileName(dir, test_name, absl::GetFlag(FLAGS_binary)), &data);
  if (status.code() == google::rpc::Code::OK) {
    return data;
  }

  // Check for the other file, just to be nice.
  if (ReadPbFile(
          GetTestCaseFileName(dir, test_name, !absl::GetFlag(FLAGS_binary)),
          &data)
          .code() == google::rpc::Code::OK) {
    return data;
  }

  // Die with the error for the first file.
  GOOGLE_LOG(FATAL) << status.ShortDebugString();
}

std::string TestDataParamName::operator()(
    const ::testing::TestParamInfo<std::pair<TestValue, TestValue>>& info)
    const {
  std::string first = info.param.first.name();
  std::string second = info.param.second.name();
  RE2::GlobalReplace(&first, RE2("[^a-zA-Z0-9_]"), "_");
  RE2::GlobalReplace(&second, RE2("[^a-zA-Z0-9_]"), "_");

  return absl::StrCat(info.index, "_", first, "_v_", second);
}

std::string TestDataParamName::operator()(
    const ::testing::TestParamInfo<TestValue>& info) const {
  std::string name = info.param.name();
  RE2::GlobalReplace(&name, RE2("[^a-zA-Z0-9_]"), "_");
  return absl::StrCat(info.index, "_", name);
}

}  // namespace testutil
}  // namespace expr
}  // namespace api
}  // namespace google
