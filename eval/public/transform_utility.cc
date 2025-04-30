#include "eval/public/transform_utility.h"

#include <string>
#include <utility>
#include <vector>

#include "cel/expr/value.pb.h"
#include "google/protobuf/any.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/arena.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "internal/proto_time_encoding.h"
#include "internal/status_macros.h"

// BEGIN_INTERNAL
#include "net/proto2/contrib/hashcode/hashcode.h"
#include "google/protobuf/util/message_differencer.h"
// END_INTERNAL

namespace google {
namespace api {
namespace expr {
namespace runtime {

absl::Status CelValueToValue(const CelValue& value, Value* result,
                             google::protobuf::Arena* arena) {
  switch (value.type()) {
    case CelValue::Type::kBool:
      result->set_bool_value(value.BoolOrDie());
      break;
    case CelValue::Type::kInt64:
      result->set_int64_value(value.Int64OrDie());
      break;
    case CelValue::Type::kUint64:
      result->set_uint64_value(value.Uint64OrDie());
      break;
    case CelValue::Type::kDouble:
      result->set_double_value(value.DoubleOrDie());
      break;
    case CelValue::Type::kString:
      result->set_string_value(value.StringOrDie().value().data(),
                               value.StringOrDie().value().size());
      break;
    case CelValue::Type::kBytes:
      result->set_bytes_value(value.BytesOrDie().value().data(),
                              value.BytesOrDie().value().size());
      break;
    case CelValue::Type::kDuration: {
      google::protobuf::Duration duration;
      auto status =
          cel::internal::EncodeDuration(value.DurationOrDie(), &duration);
      if (!status.ok()) {
        return status;
      }
      result->mutable_object_value()->PackFrom(duration);
      break;
    }
    case CelValue::Type::kTimestamp: {
      google::protobuf::Timestamp timestamp;
      auto status =
          cel::internal::EncodeTime(value.TimestampOrDie(), &timestamp);
      if (!status.ok()) {
        return status;
      }
      result->mutable_object_value()->PackFrom(timestamp);
      break;
    }
    case CelValue::Type::kNullType:
      result->set_null_value(google::protobuf::NullValue::NULL_VALUE);
      break;
    case CelValue::Type::kMessage:
      if (value.IsNull()) {
        result->set_null_value(google::protobuf::NullValue::NULL_VALUE);
      } else {
        result->mutable_object_value()->PackFrom(*value.MessageOrDie());
      }
      break;
    case CelValue::Type::kList: {
      auto& list = *value.ListOrDie();
      auto* list_value = result->mutable_list_value();
      for (int i = 0; i < list.size(); ++i) {
        CEL_RETURN_IF_ERROR(CelValueToValue(list.Get(arena, i),
                                            list_value->add_values(), arena));
      }
      break;
    }
    case CelValue::Type::kMap: {
      auto* map_value = result->mutable_map_value();
      auto& cel_map = *value.MapOrDie();
      CEL_ASSIGN_OR_RETURN(const auto* keys, cel_map.ListKeys(arena));
      for (int i = 0; i < keys->size(); ++i) {
        CelValue key = (*keys).Get(arena, i);
        auto* entry = map_value->add_entries();
        CEL_RETURN_IF_ERROR(CelValueToValue(key, entry->mutable_key(), arena));
        auto optional_value = cel_map.Get(arena, key);
        if (!optional_value) {
          return absl::Status(absl::StatusCode::kInternal,
                              "key not found in map");
        }
        CEL_RETURN_IF_ERROR(
            CelValueToValue(*optional_value, entry->mutable_value(), arena));
      }
      break;
    }
    case CelValue::Type::kError:
      // TODO(issues/87): Migrate to google.api.expr.ExprValue
      result->set_string_value("CelValue::Type::kError");
      break;
    case CelValue::Type::kCelType:
      result->set_type_value(value.CelTypeOrDie().value().data(),
                             value.CelTypeOrDie().value().size());
      break;
    case CelValue::Type::kAny:
      // kAny is a special value used in function descriptors.
      return absl::Status(absl::StatusCode::kInternal,
                          "CelValue has type kAny");
    default:
      return absl::Status(
          absl::StatusCode::kUnimplemented,
          absl::StrCat("Can't convert ", CelValue::TypeName(value.type()),
                       " to Constant."));
  }
  return absl::OkStatus();
}

absl::StatusOr<CelValue> ValueToCelValue(const Value& value,
                                         google::protobuf::Arena* arena) {
  switch (value.kind_case()) {
    case Value::kBoolValue:
      return CelValue::CreateBool(value.bool_value());
    case Value::kBytesValue:
      return CelValue::CreateBytes(CelValue::BytesHolder(
          arena->Create<std::string>(arena, value.bytes_value())));
    case Value::kDoubleValue:
      return CelValue::CreateDouble(value.double_value());
    case Value::kEnumValue:
      return CelValue::CreateInt64(value.enum_value().value());
    case Value::kInt64Value:
      return CelValue::CreateInt64(value.int64_value());
    case Value::kListValue: {
      std::vector<CelValue> list;
      for (const auto& subvalue : value.list_value().values()) {
        CEL_ASSIGN_OR_RETURN(auto list_value, ValueToCelValue(subvalue, arena));
        list.push_back(list_value);
      }
      return CelValue::CreateList(
          arena->Create<ContainerBackedListImpl>(arena, list));
    }
    case Value::kMapValue: {
      std::vector<std::pair<CelValue, CelValue>> key_values;
      for (const auto& entry : value.map_value().entries()) {
        CEL_ASSIGN_OR_RETURN(auto map_key, ValueToCelValue(entry.key(), arena));
        CEL_RETURN_IF_ERROR(CelValue::CheckMapKeyType(map_key));
        CEL_ASSIGN_OR_RETURN(auto map_value,
                             ValueToCelValue(entry.value(), arena));
        key_values.push_back(std::pair<CelValue, CelValue>(map_key, map_value));
      }
      CEL_ASSIGN_OR_RETURN(
          auto cel_map,
          CreateContainerBackedMap(absl::Span<std::pair<CelValue, CelValue>>(
              key_values.data(), key_values.size())));
      auto* cel_map_ptr = cel_map.release();
      arena->Own(cel_map_ptr);
      return CelValue::CreateMap(cel_map_ptr);
    }
    case Value::kNullValue:
      return CelValue::CreateNull();
    case Value::kObjectValue: {
      auto cel_value =
          CelProtoWrapper::CreateMessage(&value.object_value(), arena);
      if (cel_value.IsError()) return *cel_value.ErrorOrDie();
      return cel_value;
    }
    case Value::kStringValue:
      return CelValue::CreateString(CelValue::StringHolder(
          arena->Create<std::string>(arena, value.string_value())));
    case Value::kTypeValue:
      return CelValue::CreateCelType(CelValue::CelTypeHolder(
          arena->Create<std::string>(arena, value.type_value())));
    case Value::kUint64Value:
      return CelValue::CreateUint64(value.uint64_value());
    case Value::KIND_NOT_SET:
    default:
      return absl::InvalidArgumentError("Value proto is not set");
  }
}

// BEGIN_INTERNAL

size_t ValueInterner::operator()(const Value& value) const {
  using Mode = proto2::contrib::hashcode::Mode;
  // Return a conservative hash.
  static int mode =
      Mode::USE_FIELDNUMBER | Mode::USE_VALUES | Mode::IGNORE_MAP_KEY_ORDER;
  return proto2::contrib::hashcode::HashMessage(value, mode);
}

bool ValueInterner::operator()(const Value& lhs, const Value& rhs) const {
  static google::protobuf::util::MessageDifferencer* differencer = []() {
    auto* field_comparator = new google::protobuf::util::DefaultFieldComparator();
    field_comparator->set_float_comparison(
        google::protobuf::util::DefaultFieldComparator::EXACT);
    field_comparator->set_treat_nan_as_equal(true);

    auto* differencer = new google::protobuf::util::MessageDifferencer();
    auto map_entry_field = Value::descriptor()
                               ->FindFieldByName("map_value")
                               ->message_type()
                               ->FindFieldByName("entries");
    auto key_field = map_entry_field->message_type()->FindFieldByName("key");
    differencer->TreatAsMap(map_entry_field, key_field);
    differencer->set_field_comparator(field_comparator);
    return differencer;
  }();
  return differencer->Compare(lhs, rhs);
}

// END_INTERNAL

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
