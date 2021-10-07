#include "eval/public/cel_value.h"

#include <cstdint>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"

namespace google::api::expr::runtime {

namespace {

using google::protobuf::Arena;

constexpr char kErrNoMatchingOverload[] = "No matching overloads found";
constexpr char kErrNoSuchField[] = "no_such_field";
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

constexpr absl::string_view kNullTypeName = "null_type";
constexpr absl::string_view kBoolTypeName = "bool";
constexpr absl::string_view kInt64TypeName = "int";
constexpr absl::string_view kUInt64TypeName = "uint";
constexpr absl::string_view kDoubleTypeName = "double";
constexpr absl::string_view kStringTypeName = "string";
constexpr absl::string_view kBytesTypeName = "bytes";
constexpr absl::string_view kDurationTypeName = "google.protobuf.Duration";
constexpr absl::string_view kTimestampTypeName = "google.protobuf.Timestamp";
// Leading "." to prevent potential namespace clash.
constexpr absl::string_view kListTypeName = "list";
constexpr absl::string_view kMapTypeName = "map";
constexpr absl::string_view kCelTypeTypeName = "type";

// Exclusive bounds for valid duration values.
constexpr absl::Duration kDurationHigh = absl::Seconds(315576000001);
constexpr absl::Duration kDurationLow = absl::Seconds(-315576000001);

const absl::Status* DurationOverflowError() {
  static const auto* const kDurationOverflow = new absl::Status(
      absl::StatusCode::kInvalidArgument, "Duration is out of range");
  return kDurationOverflow;
}

struct DebugStringVisitor {
  std::string operator()(bool arg) { return absl::StrFormat("%d", arg); }
  std::string operator()(int64_t arg) { return absl::StrFormat("%lld", arg); }
  std::string operator()(uint64_t arg) { return absl::StrFormat("%llu", arg); }
  std::string operator()(double arg) { return absl::StrFormat("%f", arg); }

  std::string operator()(CelValue::StringHolder arg) {
    return absl::StrFormat("%s", arg.value());
  }

  std::string operator()(CelValue::BytesHolder arg) {
    return absl::StrFormat("%s", arg.value());
  }

  std::string operator()(const google::protobuf::Message* arg) {
    return arg == nullptr ? "NULL" : arg->ShortDebugString();
  }

  std::string operator()(absl::Duration arg) {
    return absl::FormatDuration(arg);
  }

  std::string operator()(absl::Time arg) {
    return absl::FormatTime(arg, absl::UTCTimeZone());
  }

  std::string operator()(const CelList* arg) {
    std::vector<std::string> elements;
    elements.reserve(arg->size());
    for (int i = 0; i < arg->size(); i++) {
      elements.push_back(arg->operator[](i).DebugString());
    }
    return absl::StrCat("[", absl::StrJoin(elements, ", "), "]");
  }

  std::string operator()(const CelMap* arg) {
    const CelList* keys = arg->ListKeys();
    std::vector<std::string> elements;
    elements.reserve(keys->size());
    for (int i = 0; i < keys->size(); i++) {
      const auto& key = (*keys)[i];
      const auto& optional_value = arg->operator[](key);
      elements.push_back(absl::StrCat("<", key.DebugString(), ">: <",
                                      optional_value.has_value()
                                          ? optional_value->DebugString()
                                          : "nullopt",
                                      ">"));
    }
    return absl::StrCat("{", absl::StrJoin(elements, ", "), "}");
  }

  std::string operator()(const UnknownSet* arg) {
    return "?";  // Not implemented.
  }

  std::string operator()(CelValue::CelTypeHolder arg) {
    return absl::StrCat(arg.value());
  }

  std::string operator()(const CelError* arg) { return arg->ToString(); }
};

}  // namespace

CelValue CelValue::CreateDuration(absl::Duration value) {
  if (value >= kDurationHigh || value <= kDurationLow) {
    return CelValue(DurationOverflowError());
  }
  return CelValue(value);
}

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
    case Type::kCelType:
      return "CelType";
    case Type::kUnknownSet:
      return "UnknownSet";
    case Type::kError:
      return "CelError";
    case Type::kAny:
      return "Any type";
    default:
      return "unknown";
  }
}

absl::Status CelValue::CheckMapKeyType(const CelValue& key) {
  switch (key.type()) {
    case CelValue::Type::kString:
    case CelValue::Type::kInt64:
    case CelValue::Type::kUint64:
    case CelValue::Type::kBool:
      return absl::OkStatus();
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "Invalid map key type: '", CelValue::TypeName(key.type()), "'"));
  }
}

CelValue CelValue::ObtainCelType() const {
  switch (type()) {
    case Type::kBool:
      return CreateCelType(CelTypeHolder(kBoolTypeName));
    case Type::kInt64:
      return CreateCelType(CelTypeHolder(kInt64TypeName));
    case Type::kUint64:
      return CreateCelType(CelTypeHolder(kUInt64TypeName));
    case Type::kDouble:
      return CreateCelType(CelTypeHolder(kDoubleTypeName));
    case Type::kString:
      return CreateCelType(CelTypeHolder(kStringTypeName));
    case Type::kBytes:
      return CreateCelType(CelTypeHolder(kBytesTypeName));
    case Type::kMessage: {
      auto msg = MessageOrDie();
      if (msg == nullptr) {
        return CreateCelType(CelTypeHolder(kNullTypeName));
      }
      // Descritptor::full_name() returns const reference, so using pointer
      // should be safe.
      return CreateCelType(CelTypeHolder(msg->GetDescriptor()->full_name()));
    }
    case Type::kDuration:
      return CreateCelType(CelTypeHolder(kDurationTypeName));
    case Type::kTimestamp:
      return CreateCelType(CelTypeHolder(kTimestampTypeName));
    case Type::kList:
      return CreateCelType(CelTypeHolder(kListTypeName));
    case Type::kMap:
      return CreateCelType(CelTypeHolder(kMapTypeName));
    case Type::kCelType:
      return CreateCelType(CelTypeHolder(kCelTypeTypeName));
    case Type::kUnknownSet:
      return *this;
    case Type::kError:
      return *this;
    default: {
      static const CelError* invalid_type_error =
          new CelError(absl::InvalidArgumentError("Unsupported CelValue type"));
      return CreateError(invalid_type_error);
    }
  }
}

// Returns debug string describing a value
const std::string CelValue::DebugString() const {
  return absl::StrCat(CelValue::TypeName(type()), ": ",
                      Visit<std::string>(DebugStringVisitor()));
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

CelValue CreateNoSuchFieldError(google::protobuf::Arena* arena, absl::string_view field) {
  return CreateErrorValue(
      arena, absl::StrCat(kErrNoSuchField, !field.empty() ? " : " : "", field),
      absl::StatusCode::kNotFound);
}

CelValue CreateNoSuchKeyError(google::protobuf::Arena* arena, absl::string_view key) {
  return CreateErrorValue(arena, absl::StrCat(kErrNoSuchKey, " : ", key),
                          absl::StatusCode::kNotFound);
}

bool CheckNoSuchKeyError(CelValue value) {
  return value.IsError() &&
         absl::StartsWith(value.ErrorOrDie()->message(), kErrNoSuchKey);
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

}  // namespace google::api::expr::runtime
