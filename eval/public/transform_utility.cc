#include "eval/public/transform_utility.h"

#include "google/api/expr/v1alpha1/value.pb.h"
#include "google/protobuf/any.pb.h"
#include "google/protobuf/struct.pb.h"
#include "net/proto2/contrib/hashcode/hashcode.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/util/message_differencer.h"
#include "absl/status/status.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "internal/proto_util.h"
#include "base/status_macros.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

absl::Status CelValueToValue(const CelValue& value, Value* result) {
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
      result->set_string_value(value.StringOrDie().value());
      break;
    case CelValue::Type::kBytes:
      result->set_bytes_value(value.BytesOrDie().value());
      break;
    case CelValue::Type::kDuration: {
      google::protobuf::Duration duration;
      expr::internal::EncodeDuration(value.DurationOrDie(), &duration);
      result->mutable_object_value()->PackFrom(duration);
      break;
    }
    case CelValue::Type::kTimestamp: {
      google::protobuf::Timestamp timestamp;
      expr::internal::EncodeTime(value.TimestampOrDie(), &timestamp);
      result->mutable_object_value()->PackFrom(timestamp);
      break;
    }
    case CelValue::Type::kMessage:
      if (value.MessageOrDie() == nullptr) {
        result->set_null_value(google::protobuf::NullValue::NULL_VALUE);
      } else {
        result->mutable_object_value()->PackFrom(*value.MessageOrDie());
      }
      break;
    case CelValue::Type::kList: {
      auto& list = *value.ListOrDie();
      auto* list_value = result->mutable_list_value();
      for (int i = 0; i < list.size(); ++i) {
        RETURN_IF_ERROR(CelValueToValue(list[i], list_value->add_values()));
      }
      break;
    }
    case CelValue::Type::kMap: {
      google::api::expr::MapValue* map_value = result->mutable_map_value();
      auto& cel_map = *value.MapOrDie();
      const auto& keys = *cel_map.ListKeys();
      for (int i = 0; i < keys.size(); ++i) {
        CelValue key = keys[i];
        auto* entry = map_value->add_entries();
        RETURN_IF_ERROR(CelValueToValue(key, entry->mutable_key()));
        auto optional_value = cel_map[key];
        if (!optional_value) {
          return absl::Status(absl::StatusCode::kInternal,
                              "key not found in map");
        }
        RETURN_IF_ERROR(
            CelValueToValue(optional_value.value(), entry->mutable_value()));
      }
      break;
    }
    case CelValue::Type::kError:
      // TODO(issues/87): Migrate to google.api.expr.ExprValue
      result->set_string_value("CelValue::Type::kError");
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
    case Value::KIND_NOT_SET:
      return absl::InvalidArgumentError("Value proto is not set");
    case Value::kBoolValue:
      return CelValue::CreateBool(value.bool_value());
    case Value::kBytesValue:
      return CelValue::CreateBytes(CelValue::BytesHolder(&value.bytes_value()));
    case Value::kDoubleValue:
      return CelValue::CreateDouble(value.double_value());
    case Value::kEnumValue:
      return CelValue::CreateInt64(value.enum_value().value());
    case Value::kInt64Value:
      return CelValue::CreateInt64(value.int64_value());
    case Value::kListValue: {
      std::vector<CelValue> list;
      for (const auto& subvalue : value.list_value().values()) {
        ASSIGN_OR_RETURN(auto list_value, ValueToCelValue(subvalue, arena));
        list.push_back(list_value);
      }
      return CelValue::CreateList(
          arena->Create<ContainerBackedListImpl>(arena, list));
    }
    case Value::kMapValue: {
      std::vector<std::pair<CelValue, CelValue>> key_values;
      for (const auto& entry : value.map_value().entries()) {
        ASSIGN_OR_RETURN(auto map_key, ValueToCelValue(entry.key(), arena));
        ASSIGN_OR_RETURN(auto map_value, ValueToCelValue(entry.value(), arena));
        key_values.push_back(std::pair<CelValue, CelValue>(map_key, map_value));
      }
      auto cel_map =
          CreateContainerBackedMap(absl::Span<std::pair<CelValue, CelValue>>(
                                       key_values.data(), key_values.size()))
              .release();
      arena->Own(cel_map);
      return CelValue::CreateMap(cel_map);
    }
    case Value::kNullValue:
      return CelValue::CreateNull();
    case Value::kObjectValue:
      return CelProtoWrapper::CreateMessage(&value.object_value(), arena);
    case Value::kStringValue:
      return CelValue::CreateString(
          CelValue::StringHolder(&value.string_value()));
    case Value::kTypeValue:
      return CelValue::CreateString(
          CelValue::StringHolder(&value.type_value()));
    case Value::kUint64Value:
      return CelValue::CreateUint64(value.uint64_value());
  }
}

size_t ValueInterner::operator()(const Value& value) const {
  using Mode = google::protobuf::contrib::hashcode::Mode;
  // Return a conservative hash.
  static int mode =
      Mode::USE_FIELDNUMBER | Mode::USE_VALUES | Mode::IGNORE_MAP_KEY_ORDER;
  return google::protobuf::contrib::hashcode::HashMessage(value, mode);
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

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
