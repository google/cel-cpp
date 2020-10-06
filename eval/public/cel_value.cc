#include "eval/public/cel_value.h"

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using google::protobuf::Arena;

constexpr char kErrNoMatchingOverload[] = "No matching overloads found";
constexpr char kErrNoSuchKey[] = "Key not found in map";
constexpr absl::string_view kErrUnknownValue = "Unknown value ";
// Error name for MissingAttributeError indicating that evaluation has
// accessed an attribute whose value is undefined. go/terminal-unknown
constexpr absl::string_view kErrMissingAttribute = "MissingAttributeError: ";
constexpr absl::string_view kPayloadUrlUnknownPath = "unknown_path";
constexpr absl::string_view kPayloadUrlMissingAttributePath =
    "missing_attribute_path";
constexpr absl::string_view kPayloadUrlUnknownFunctionResult =
    "cel_is_unknown_function_result";

}  // namespace

std::string CelValue::TypeName(Type value_type) {
  switch (value_type) {
    case Type::kBool:
      return "bool";
    case Type::kInt64:
      return "int64";
    case Type::kUint64:
      return "uint64";
    case Type::kDouble:
      return "double";
    case Type::kString:
      return "string";
    case Type::kBytes:
      return "bytes";
    case Type::kMessage:
      return "Message";
    case Type::kDuration:
      return "Duration";
    case Type::kTimestamp:
      return "Timestamp";
    case Type::kList:
      return "CelList";
    case Type::kMap:
      return "CelMap";
    case Type::kUnknownSet:
      return "UnknownSet";
    case Type::kError:
      return "CelError";
    default:
      return "Unknown Type";
  }
}

// Returns debug string describing a value
const std::string CelValue::DebugString() const {
  switch (type()) {
    case Type::kBool:
      return absl::StrFormat("bool: %d", BoolOrDie());
    case Type::kInt64:
      return absl::StrFormat("int64: %lld", Int64OrDie());
    case Type::kUint64:
      return absl::StrFormat("uint64: %llu", Uint64OrDie());
    case Type::kDouble:
      return absl::StrFormat("double: %f", DoubleOrDie());
    case Type::kString:
      return absl::StrFormat("string: %s", StringOrDie().value());
    case Type::kBytes:
      return absl::StrFormat("bytes: %s", BytesOrDie().value());
    case Type::kMessage:
      return absl::StrFormat(
          "Message: %s",
          IsNull() ? "NULL" : MessageOrDie()->ShortDebugString());
    case Type::kDuration:
      return absl::StrFormat("Duration: %s",
                             absl::FormatDuration(DurationOrDie()));
    case Type::kTimestamp:
      return absl::StrFormat(
          "Time: %s", absl::FormatTime(TimestampOrDie(), absl::UTCTimeZone()));
    case Type::kList:
      return absl::StrFormat("List, size: %lld", ListOrDie()->size());
    case Type::kMap:
      return absl::StrFormat("Map, size: %lld", MapOrDie()->size());
    case Type::kUnknownSet:
      return "UnknownSet";
    case Type::kError:
      return absl::StrFormat("Error: %s", ErrorOrDie()->ToString());
    case Type::kAny:
      return "Any";
    default:
      return "unknown_type";
  }
}

CelValue CreateErrorValue(Arena* arena, absl::string_view message,
                          absl::StatusCode error_code, int) {
  CelError* error = Arena::Create<CelError>(arena, error_code, message);
  return CelValue::CreateError(error);
}

CelValue CreateNoMatchingOverloadError(google::protobuf::Arena* arena) {
  return CreateErrorValue(arena, kErrNoMatchingOverload,
                          absl::StatusCode::kUnknown);
}

CelValue CreateNoMatchingOverloadError(google::protobuf::Arena* arena,
                                       absl::string_view fn) {
  return CreateErrorValue(arena, absl::StrCat(kErrNoMatchingOverload, " ", fn),
                          absl::StatusCode::kUnknown);
}

bool CheckNoMatchingOverloadError(CelValue value) {
  return value.IsError() &&
         value.ErrorOrDie()->code() == absl::StatusCode::kUnknown &&
         absl::StrContains(value.ErrorOrDie()->message(),
                           kErrNoMatchingOverload);
}

CelValue CreateNoSuchFieldError(google::protobuf::Arena* arena) {
  return CreateErrorValue(arena, "no_such_field", absl::StatusCode::kNotFound);
}

CelValue CreateNoSuchKeyError(google::protobuf::Arena* arena, absl::string_view) {
  return CreateErrorValue(arena, kErrNoSuchKey, absl::StatusCode::kNotFound);
}

bool CheckNoSuchKeyError(CelValue value) {
  return value.IsError() && value.ErrorOrDie()->message() == kErrNoSuchKey;
}

CelValue CreateUnknownValueError(google::protobuf::Arena* arena,
                                 absl::string_view unknown_path) {
  CelError* error =
      Arena::Create<CelError>(arena, absl::StatusCode::kUnavailable,
                              absl::StrCat(kErrUnknownValue, unknown_path));
  error->SetPayload(kPayloadUrlUnknownPath, absl::Cord(unknown_path));
  return CelValue::CreateError(error);
}

bool IsUnknownValueError(const CelValue& value) {
  // TODO(issues/41): replace with the implementation of go/cel-known-unknowns
  if (!value.IsError()) return false;
  const CelError* error = value.ErrorOrDie();
  if (error && error->code() == absl::StatusCode::kUnavailable) {
    auto path = error->GetPayload(kPayloadUrlUnknownPath);
    return path.has_value();
  }
  return false;
}

CelValue CreateMissingAttributeError(google::protobuf::Arena* arena,
                                     absl::string_view missing_attribute_path) {
  CelError* error = Arena::Create<CelError>(
      arena, absl::StatusCode::kInvalidArgument,
      absl::StrCat(kErrMissingAttribute, missing_attribute_path));
  error->SetPayload(kPayloadUrlMissingAttributePath,
                    absl::Cord(missing_attribute_path));
  return CelValue::CreateError(error);
}

bool IsMissingAttributeError(const CelValue& value) {
  if (!value.IsError()) return false;
  const CelError* error = value.ErrorOrDie();  // Crash ok
  if (error && error->code() == absl::StatusCode::kInvalidArgument) {
    auto path = error->GetPayload(kPayloadUrlMissingAttributePath);
    return path.has_value();
  }
  return false;
}

std::set<std::string> GetUnknownPathsSetOrDie(const CelValue& value) {
  // TODO(issues/41): replace with the implementation of go/cel-known-unknowns
  const CelError* error = value.ErrorOrDie();
  if (error && error->code() == absl::StatusCode::kUnavailable) {
    auto path = error->GetPayload(kPayloadUrlUnknownPath);
    if (path.has_value()) return {std::string(path.value())};
  }
  GOOGLE_LOG(FATAL) << "The value is not an unknown path error.";  // Crash ok
  return {};
}

CelValue CreateUnknownFunctionResultError(google::protobuf::Arena* arena,
                                          absl::string_view help_message) {
  CelError* error = Arena::Create<CelError>(
      arena, absl::StatusCode::kUnavailable,
      absl::StrCat("Unknown function result: ", help_message));
  error->SetPayload(kPayloadUrlUnknownFunctionResult, absl::Cord("true"));
  return CelValue::CreateError(error);
}

bool IsUnknownFunctionResult(const CelValue& value) {
  if (!value.IsError()) {
    return false;
  }
  const CelError* error = value.ErrorOrDie();
  if (error == nullptr || error->code() != absl::StatusCode::kUnavailable) {
    return false;
  }
  auto payload = error->GetPayload(kPayloadUrlUnknownFunctionResult);
  return payload.has_value() && payload.value() == "true";
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
