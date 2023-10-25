#include "eval/eval/create_struct_step.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "base/memory.h"
#include "base/type.h"
#include "base/types/enum_type.h"
#include "base/types/struct_type.h"
#include "base/value_factory.h"
#include "base/values/bool_value.h"
#include "base/values/bytes_value.h"
#include "base/values/double_value.h"
#include "base/values/int_value.h"
#include "base/values/list_value.h"
#include "base/values/map_value.h"
#include "base/values/map_value_builder.h"
#include "base/values/string_value.h"
#include "base/values/struct_value.h"
#include "base/values/struct_value_builder.h"
#include "base/values/uint_value.h"
#include "base/values/unknown_value.h"
#include "common/json.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/internal/errors.h"
#include "eval/internal/interop.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/no_destructor.h"
#include "internal/overflow.h"
#include "internal/overloaded.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::BoolValue;
using ::cel::BytesValue;
using ::cel::DoubleValue;
using ::cel::EnumValue;
using ::cel::Handle;
using ::cel::IntValue;
using ::cel::ListValue;
using ::cel::MapValue;
using ::cel::StringValue;
using ::cel::StructType;
using ::cel::StructValue;
using ::cel::StructValueBuilderInterface;
using ::cel::Type;
using ::cel::TypeManager;
using ::cel::UintValue;
using ::cel::UnknownValue;
using ::cel::Value;
using ::cel::ValueFactory;
using ::cel::interop_internal::CreateErrorValueFromView;
using ::cel::interop_internal::CreateLegacyMapValue;

class WellKnownTypeValueBuilder;

// `WellKnownType` describes one of the well known types recognized by CEL and
// exposes the functionality necessary to implement `CreateStruct`.
class WellKnownType {
 public:
  virtual ~WellKnownType() = default;

  virtual absl::StatusOr<absl::optional<int64_t>> FindFieldByName(
      absl::string_view name) const = 0;

  virtual std::unique_ptr<WellKnownTypeValueBuilder> NewValueBuilder(
      ValueFactory& value_factory) const = 0;
};

absl::Status TypeConversionError(absl::string_view from, absl::string_view to) {
  return absl::InvalidArgumentError(
      absl::StrCat("Unable to convert from ", from, " to ", to));
}

// `WellKnownTypeValueBuilder` is similar to `StructValueBuilderInterface` but
// can produce a `Value` instead of `StructValue`. This is to perform the
// automatic unwrapping of the well known types to their CEL primitive type.
class WellKnownTypeValueBuilder {
 public:
  virtual ~WellKnownTypeValueBuilder() = default;

  virtual absl::Status SetFieldByNumber(int64_t number,
                                        Handle<Value> value) = 0;

  virtual absl::StatusOr<Handle<Value>> Build() && = 0;
};

// `WellKnownTypeValueBuilder` for `google.protobuf.BoolValue`.
class BoolWellKnownTypeValueBuilder final : public WellKnownTypeValueBuilder {
 public:
  explicit BoolWellKnownTypeValueBuilder(ValueFactory& value_factory)
      : value_factory_(value_factory) {}

  absl::Status SetFieldByNumber(int64_t number, Handle<Value> value) override {
    if (number != 1) {
      return cel::runtime_internal::CreateNoSuchFieldError(
          absl::StrCat(number));
    }
    if (!value->Is<BoolValue>()) {
      return TypeConversionError(value->type()->name(), "bool");
    }
    value_ = value->As<BoolValue>().value();
    return absl::OkStatus();
  }

  absl::StatusOr<Handle<Value>> Build() && override {
    return value_factory_.CreateBoolValue(value_);
  }

 private:
  ValueFactory& value_factory_;
  bool value_ = false;
};

// `WellKnownTypeValueBuilder` for `google.protobuf.Int32Value`.
class Int32WellKnownTypeValueBuilder final : public WellKnownTypeValueBuilder {
 public:
  explicit Int32WellKnownTypeValueBuilder(ValueFactory& value_factory)
      : value_factory_(value_factory) {}

  absl::Status SetFieldByNumber(int64_t number, Handle<Value> value) override {
    if (number != 1) {
      return cel::runtime_internal::CreateNoSuchFieldError(
          absl::StrCat(number));
    }
    if (!value->Is<IntValue>()) {
      return TypeConversionError(value->type()->name(), "int");
    }
    CEL_ASSIGN_OR_RETURN(value_, cel::internal::CheckedInt64ToInt32(
                                     value->As<IntValue>().value()));
    return absl::OkStatus();
  }

  absl::StatusOr<Handle<Value>> Build() && override {
    return value_factory_.CreateIntValue(value_);
  }

 private:
  ValueFactory& value_factory_;
  int32_t value_ = 0;
};

// `WellKnownTypeValueBuilder` for `google.protobuf.Int64Value`.
class Int64WellKnownTypeValueBuilder final : public WellKnownTypeValueBuilder {
 public:
  explicit Int64WellKnownTypeValueBuilder(ValueFactory& value_factory)
      : value_factory_(value_factory) {}

  absl::Status SetFieldByNumber(int64_t number, Handle<Value> value) override {
    if (number != 1) {
      return cel::runtime_internal::CreateNoSuchFieldError(
          absl::StrCat(number));
    }
    if (!value->Is<IntValue>()) {
      return TypeConversionError(value->type()->name(), "int");
    }
    value_ = value->As<IntValue>().value();
    return absl::OkStatus();
  }

  absl::StatusOr<Handle<Value>> Build() && override {
    return value_factory_.CreateIntValue(value_);
  }

 private:
  ValueFactory& value_factory_;
  int64_t value_ = 0;
};

// `WellKnownTypeValueBuilder` for `google.protobuf.UInt32Value`.
class UInt32WellKnownTypeValueBuilder final : public WellKnownTypeValueBuilder {
 public:
  explicit UInt32WellKnownTypeValueBuilder(ValueFactory& value_factory)
      : value_factory_(value_factory) {}

  absl::Status SetFieldByNumber(int64_t number, Handle<Value> value) override {
    if (number != 1) {
      return cel::runtime_internal::CreateNoSuchFieldError(
          absl::StrCat(number));
    }
    if (!value->Is<UintValue>()) {
      return TypeConversionError(value->type()->name(), "uint");
    }
    CEL_ASSIGN_OR_RETURN(value_, cel::internal::CheckedUint64ToUint32(
                                     value->As<UintValue>().value()));
    return absl::OkStatus();
  }

  absl::StatusOr<Handle<Value>> Build() && override {
    return value_factory_.CreateUintValue(value_);
  }

 private:
  ValueFactory& value_factory_;
  uint32_t value_ = 0;
};

// `WellKnownTypeValueBuilder` for `google.protobuf.UInt64Value`.
class UInt64WellKnownTypeValueBuilder final : public WellKnownTypeValueBuilder {
 public:
  explicit UInt64WellKnownTypeValueBuilder(ValueFactory& value_factory)
      : value_factory_(value_factory) {}

  absl::Status SetFieldByNumber(int64_t number, Handle<Value> value) override {
    if (number != 1) {
      return cel::runtime_internal::CreateNoSuchFieldError(
          absl::StrCat(number));
    }
    if (!value->Is<UintValue>()) {
      return TypeConversionError(value->type()->name(), "uint");
    }
    value_ = value->As<UintValue>().value();
    return absl::OkStatus();
  }

  absl::StatusOr<Handle<Value>> Build() && override {
    return value_factory_.CreateUintValue(value_);
  }

 private:
  ValueFactory& value_factory_;
  uint64_t value_ = 0;
};

// `WellKnownTypeValueBuilder` for `google.protobuf.FloatValue`.
class FloatWellKnownTypeValueBuilder final : public WellKnownTypeValueBuilder {
 public:
  explicit FloatWellKnownTypeValueBuilder(ValueFactory& value_factory)
      : value_factory_(value_factory) {}

  absl::Status SetFieldByNumber(int64_t number, Handle<Value> value) override {
    if (number != 1) {
      return cel::runtime_internal::CreateNoSuchFieldError(
          absl::StrCat(number));
    }
    if (!value->Is<DoubleValue>()) {
      return TypeConversionError(value->type()->name(), "double");
    }
    value_ = static_cast<float>(value->As<DoubleValue>().value());
    return absl::OkStatus();
  }

  absl::StatusOr<Handle<Value>> Build() && override {
    return value_factory_.CreateDoubleValue(value_);
  }

 private:
  ValueFactory& value_factory_;
  float value_ = 0;
};

// `WellKnownTypeValueBuilder` for `google.protobuf.DoubleValue`.
class DoubleWellKnownTypeValueBuilder final : public WellKnownTypeValueBuilder {
 public:
  explicit DoubleWellKnownTypeValueBuilder(ValueFactory& value_factory)
      : value_factory_(value_factory) {}

  absl::Status SetFieldByNumber(int64_t number, Handle<Value> value) override {
    if (number != 1) {
      return cel::runtime_internal::CreateNoSuchFieldError(
          absl::StrCat(number));
    }
    if (!value->Is<DoubleValue>()) {
      return TypeConversionError(value->type()->name(), "double");
    }
    value_ = value->As<DoubleValue>().value();
    return absl::OkStatus();
  }

  absl::StatusOr<Handle<Value>> Build() && override {
    return value_factory_.CreateDoubleValue(value_);
  }

 private:
  ValueFactory& value_factory_;
  double value_ = 0;
};

// `WellKnownTypeValueBuilder` for `google.protobuf.StringValue`.
class StringWellKnownTypeValueBuilder final : public WellKnownTypeValueBuilder {
 public:
  explicit StringWellKnownTypeValueBuilder(ValueFactory& value_factory)
      : value_factory_(value_factory), value_(value_factory.GetStringValue()) {}

  absl::Status SetFieldByNumber(int64_t number, Handle<Value> value) override {
    if (number != 1) {
      return cel::runtime_internal::CreateNoSuchFieldError(
          absl::StrCat(number));
    }
    if (!value->Is<StringValue>()) {
      return TypeConversionError(value->type()->name(), "string");
    }
    value_ = std::move(value).As<StringValue>();
    return absl::OkStatus();
  }

  absl::StatusOr<Handle<Value>> Build() && override {
    return std::move(value_);
  }

 private:
  ValueFactory& value_factory_;
  Handle<StringValue> value_;
};

// `WellKnownTypeValueBuilder` for `google.protobuf.BytesValue`.
class BytesWellKnownTypeValueBuilder final : public WellKnownTypeValueBuilder {
 public:
  explicit BytesWellKnownTypeValueBuilder(ValueFactory& value_factory)
      : value_factory_(value_factory), value_(value_factory_.GetBytesValue()) {}

  absl::Status SetFieldByNumber(int64_t number, Handle<Value> value) override {
    if (number != 1) {
      return cel::runtime_internal::CreateNoSuchFieldError(
          absl::StrCat(number));
    }
    if (!value->Is<BytesValue>()) {
      return TypeConversionError(value->type()->name(), "bytes");
    }
    value_ = std::move(value).As<BytesValue>();
    return absl::OkStatus();
  }

  absl::StatusOr<Handle<Value>> Build() && override {
    return std::move(value_);
  }

 private:
  ValueFactory& value_factory_;
  Handle<BytesValue> value_;
};

// `WellKnownTypeValueBuilder` for `google.protobuf.Duration`.
class DurationWellKnownTypeValueBuilder final
    : public WellKnownTypeValueBuilder {
 public:
  explicit DurationWellKnownTypeValueBuilder(ValueFactory& value_factory)
      : value_factory_(value_factory) {}

  absl::Status SetFieldByNumber(int64_t number, Handle<Value> value) override {
    if (number == 1) {
      if (!value->Is<IntValue>()) {
        return TypeConversionError(value->type()->name(), "int");
      }
      seconds_ = value->As<IntValue>().value();
      return absl::OkStatus();
    }
    if (number == 2) {
      if (!value->Is<IntValue>()) {
        return TypeConversionError(value->type()->name(), "int");
      }
      CEL_ASSIGN_OR_RETURN(nanos_, cel::internal::CheckedInt64ToInt32(
                                       value->As<IntValue>().value()));
      return absl::OkStatus();
    }
    return cel::runtime_internal::CreateNoSuchFieldError(absl::StrCat(number));
  }

  absl::StatusOr<Handle<Value>> Build() && override {
    return value_factory_.CreateDurationValue(absl::Seconds(seconds_) +
                                              absl::Nanoseconds(nanos_));
  }

 private:
  ValueFactory& value_factory_;
  int64_t seconds_ = 0;
  int32_t nanos_ = 0;
};

// `WellKnownTypeValueBuilder` for `google.protobuf.Timestamp`.
class TimestampWellKnownTypeValueBuilder final
    : public WellKnownTypeValueBuilder {
 public:
  explicit TimestampWellKnownTypeValueBuilder(ValueFactory& value_factory)
      : value_factory_(value_factory) {}

  absl::Status SetFieldByNumber(int64_t number, Handle<Value> value) override {
    if (number == 1) {
      if (!value->Is<IntValue>()) {
        return TypeConversionError(value->type()->name(), "int");
      }
      seconds_ = value->As<IntValue>().value();
      return absl::OkStatus();
    }
    if (number == 2) {
      if (!value->Is<IntValue>()) {
        return TypeConversionError(value->type()->name(), "int");
      }
      CEL_ASSIGN_OR_RETURN(nanos_, cel::internal::CheckedInt64ToInt32(
                                       value->As<IntValue>().value()));
      return absl::OkStatus();
    }
    return cel::runtime_internal::CreateNoSuchFieldError(absl::StrCat(number));
  }

  absl::StatusOr<Handle<Value>> Build() && override {
    return value_factory_.CreateTimestampValue(absl::UnixEpoch() +
                                               absl::Seconds(seconds_) +
                                               absl::Nanoseconds(nanos_));
  }

 private:
  ValueFactory& value_factory_;
  int64_t seconds_ = 0;
  int32_t nanos_ = 0;
};

// `WellKnownTypeValueBuilder` for `google.protobuf.Any`.
class AnyWellKnownTypeValueBuilder final : public WellKnownTypeValueBuilder {
 public:
  explicit AnyWellKnownTypeValueBuilder(ValueFactory& value_factory)
      : value_factory_(value_factory) {}

  absl::Status SetFieldByNumber(int64_t number, Handle<Value> value) override {
    if (number == 1) {
      if (!value->Is<StringValue>()) {
        return TypeConversionError(value->type()->name(), "string");
      }
      type_url_ = std::move(value).As<StringValue>()->ToString();
      return absl::OkStatus();
    }
    if (number == 2) {
      if (!value->Is<BytesValue>()) {
        return TypeConversionError(value->type()->name(), "bytes");
      }
      value_ = std::move(value).As<BytesValue>()->ToCord();
      return absl::OkStatus();
    }
    return cel::runtime_internal::CreateNoSuchFieldError(absl::StrCat(number));
  }

  absl::StatusOr<Handle<Value>> Build() && override {
    CEL_ASSIGN_OR_RETURN(
        auto type, value_factory_.type_manager().ResolveType(absl::StripPrefix(
                       type_url_, cel::kTypeGoogleApisComPrefix)));
    if (!type.has_value()) {
      return absl::NotFoundError(absl::StrCat(
          "unable to deserialize google.protobuf.Any: type not found: ",
          type_url_));
    }
    return (*type)->NewValueFromAny(value_factory_, value_);
  }

 private:
  ValueFactory& value_factory_;
  std::string type_url_;
  absl::Cord value_;
};

// `WellKnownTypeValueBuilder` for `google.protobuf.Value`.
class ValueWellKnownTypeValueBuilder final : public WellKnownTypeValueBuilder {
 public:
  explicit ValueWellKnownTypeValueBuilder(ValueFactory& value_factory)
      : value_factory_(value_factory), value_(cel::kJsonNull) {}

  absl::Status SetFieldByNumber(int64_t number, Handle<Value> value) override {
    switch (number) {
      case 1:
        if (value->Is<EnumValue>()) {
          if (value->type().As<cel::EnumType>()->name() !=
              "google.protobuf.NullValue") {
            return TypeConversionError(value->type()->name(),
                                       "google.protobuf.NullValue");
          }
          value_ = cel::kJsonNull;
          return absl::OkStatus();
        }
        if (value->Is<IntValue>()) {
          value_ = cel::kJsonNull;
          return absl::OkStatus();
        }
        return TypeConversionError(value->type()->name(),
                                   "google.protobuf.NullValue");
      case 2:
        if (!value->Is<DoubleValue>()) {
          return TypeConversionError(value->type()->name(), "double");
        }
        value_ = value->As<DoubleValue>().value();
        return absl::OkStatus();
      case 3:
        if (!value->Is<StringValue>()) {
          return TypeConversionError(value->type()->name(), "string");
        }
        value_ = value->As<StringValue>().ToCord();
        return absl::OkStatus();
      case 4:
        if (!value->Is<BoolValue>()) {
          return TypeConversionError(value->type()->name(), "bool");
        }
        value_ = value->As<BoolValue>().value();
        return absl::OkStatus();
      case 5: {
        if (value->Is<MapValue>()) {
          CEL_ASSIGN_OR_RETURN(
              value_,
              value->As<MapValue>().ConvertToJsonObject(value_factory_));
          return absl::OkStatus();
        }
        if (value->Is<StructValue>()) {
          CEL_ASSIGN_OR_RETURN(
              auto json,
              value->As<StructValue>().ConvertToJson(value_factory_));
          if (!absl::holds_alternative<cel::JsonObject>(json)) {
            return TypeConversionError(value->type()->name(),
                                       "map<string, dyn>");
          }
          value_ = absl::get<cel::JsonObject>(std::move(json));
          return absl::OkStatus();
        }
        return TypeConversionError(value->type()->name(), "map<string, dyn>");
      }
      case 6: {
        if (!value->Is<ListValue>()) {
          return TypeConversionError(value->type()->name(), "list<dyn>");
        }
        CEL_ASSIGN_OR_RETURN(
            value_, value->As<ListValue>().ConvertToJsonArray(value_factory_));
        return absl::OkStatus();
      }
      default:
        return cel::runtime_internal::CreateNoSuchFieldError(
            absl::StrCat(number));
    }
  }

  absl::StatusOr<Handle<Value>> Build() && override {
    return value_factory_.CreateValueFromJson(std::move(value_));
  }

 private:
  ValueFactory& value_factory_;
  cel::Json value_;
};

// `WellKnownTypeValueBuilder` for `google.protobuf.ListValue`.
class ListWellKnownTypeValueBuilder final : public WellKnownTypeValueBuilder {
 public:
  explicit ListWellKnownTypeValueBuilder(ValueFactory& value_factory)
      : value_factory_(value_factory) {}

  absl::Status SetFieldByNumber(int64_t number, Handle<Value> value) override {
    if (number != 1) {
      return cel::runtime_internal::CreateNoSuchFieldError(
          absl::StrCat(number));
    }
    if (!value->Is<ListValue>()) {
      return TypeConversionError(value->type()->name(), "list<dyn>");
    }
    CEL_ASSIGN_OR_RETURN(
        value_, value->As<ListValue>().ConvertToJsonArray(value_factory_));
    return absl::OkStatus();
  }

  absl::StatusOr<Handle<Value>> Build() && override {
    return value_factory_.CreateListValueFromJson(std::move(value_));
  }

 private:
  ValueFactory& value_factory_;
  cel::JsonArray value_;
};

// `WellKnownTypeValueBuilder` for `google.protobuf.Struct`.
class StructWellKnownTypeValueBuilder final : public WellKnownTypeValueBuilder {
 public:
  explicit StructWellKnownTypeValueBuilder(ValueFactory& value_factory)
      : value_factory_(value_factory) {}

  absl::Status SetFieldByNumber(int64_t number, Handle<Value> value) override {
    if (number != 1) {
      return cel::runtime_internal::CreateNoSuchFieldError(
          absl::StrCat(number));
    }
    if (value->Is<MapValue>()) {
      CEL_ASSIGN_OR_RETURN(
          value_, value->As<MapValue>().ConvertToJsonObject(value_factory_));
      return absl::OkStatus();
    }
    if (value->Is<StructValue>()) {
      CEL_ASSIGN_OR_RETURN(
          auto json, value->As<StructValue>().ConvertToJson(value_factory_));
      if (!absl::holds_alternative<cel::JsonObject>(json)) {
        return TypeConversionError(value->type()->name(), "map<string, dyn>");
      }
      value_ = absl::get<cel::JsonObject>(std::move(json));
      return absl::OkStatus();
    }
    return TypeConversionError(value->type()->name(), "map<string, dyn>");
  }

  absl::StatusOr<Handle<Value>> Build() && override {
    return value_factory_.CreateMapValueFromJson(std::move(value_));
  }

 private:
  ValueFactory& value_factory_;
  cel::JsonObject value_;
};

template <typename T>
class WrapperWellKnownType final : public WellKnownType {
 public:
  absl::StatusOr<absl::optional<int64_t>> FindFieldByName(
      absl::string_view name) const override {
    if (name == "value") {
      return 1;
    }
    return absl::nullopt;
  }

  std::unique_ptr<WellKnownTypeValueBuilder> NewValueBuilder(
      ValueFactory& value_factory) const override {
    return std::make_unique<T>(value_factory);
  }
};

// `WellKnownType` for `google.protobuf.BoolValue`.
using BoolWellKnownType = WrapperWellKnownType<BoolWellKnownTypeValueBuilder>;
// `WellKnownType` for `google.protobuf.Int32Value`.
using Int32WellKnownType = WrapperWellKnownType<Int32WellKnownTypeValueBuilder>;
// `WellKnownType` for `google.protobuf.Int64Value`.
using Int64WellKnownType = WrapperWellKnownType<Int64WellKnownTypeValueBuilder>;
// `WellKnownType` for `google.protobuf.UInt32Value`.
using UInt32WellKnownType =
    WrapperWellKnownType<UInt32WellKnownTypeValueBuilder>;
// `WellKnownType` for `google.protobuf.UInt64Value`.
using UInt64WellKnownType =
    WrapperWellKnownType<UInt64WellKnownTypeValueBuilder>;
// `WellKnownType` for `google.protobuf.FloatValue`.
using FloatWellKnownType = WrapperWellKnownType<FloatWellKnownTypeValueBuilder>;
// `WellKnownType` for `google.protobuf.DoubleValue`.
using DoubleWellKnownType =
    WrapperWellKnownType<DoubleWellKnownTypeValueBuilder>;
// `WellKnownType` for `google.protobuf.BytesValue`.
using BytesWellKnownType = WrapperWellKnownType<BytesWellKnownTypeValueBuilder>;
// `WellKnownType` for `google.protobuf.StringValue`.
using StringWellKnownType =
    WrapperWellKnownType<StringWellKnownTypeValueBuilder>;

template <typename T>
class DurationOrTimestampWellKnownType final : public WellKnownType {
 public:
  absl::StatusOr<absl::optional<int64_t>> FindFieldByName(
      absl::string_view name) const override {
    if (name == "seconds") {
      return 1;
    }
    if (name == "nanos") {
      return 2;
    }
    return absl::nullopt;
  }

  std::unique_ptr<WellKnownTypeValueBuilder> NewValueBuilder(
      ValueFactory& value_factory) const override {
    return std::make_unique<T>(value_factory);
  }
};

// `WellKnownType` for `google.protobuf.Duration`.
using DurationWellKnownType =
    DurationOrTimestampWellKnownType<DurationWellKnownTypeValueBuilder>;
// `WellKnownType` for `google.protobuf.Timestamp`.
using TimestampWellKnownType =
    DurationOrTimestampWellKnownType<TimestampWellKnownTypeValueBuilder>;

// `WellKnownType` for `google.protobuf.Any`.
class AnyWellKnownType final : public WellKnownType {
 public:
  absl::StatusOr<absl::optional<int64_t>> FindFieldByName(
      absl::string_view name) const override {
    if (name == "type_url") {
      return 1;
    }
    if (name == "value") {
      return 2;
    }
    return absl::nullopt;
  }

  std::unique_ptr<WellKnownTypeValueBuilder> NewValueBuilder(
      ValueFactory& value_factory) const override {
    return std::make_unique<AnyWellKnownTypeValueBuilder>(value_factory);
  }
};

// `WellKnownType` for `google.protobuf.Value`.
class ValueWellKnownType final : public WellKnownType {
 public:
  absl::StatusOr<absl::optional<int64_t>> FindFieldByName(
      absl::string_view name) const override {
    if (name == "null_value") {
      return 1;
    }
    if (name == "number_value") {
      return 2;
    }
    if (name == "string_value") {
      return 3;
    }
    if (name == "bool_value") {
      return 4;
    }
    if (name == "struct_value") {
      return 5;
    }
    if (name == "list_value") {
      return 6;
    }
    return absl::nullopt;
  }

  std::unique_ptr<WellKnownTypeValueBuilder> NewValueBuilder(
      ValueFactory& value_factory) const override {
    return std::make_unique<ValueWellKnownTypeValueBuilder>(value_factory);
  }
};

// `WellKnownType` for `google.protobuf.ListValue`.
class ListWellKnownType final : public WellKnownType {
 public:
  absl::StatusOr<absl::optional<int64_t>> FindFieldByName(
      absl::string_view name) const override {
    if (name == "values") {
      return 1;
    }
    return absl::nullopt;
  }

  std::unique_ptr<WellKnownTypeValueBuilder> NewValueBuilder(
      ValueFactory& value_factory) const override {
    return std::make_unique<ListWellKnownTypeValueBuilder>(value_factory);
  }
};

// `WellKnownType` for `google.protobuf.Struct`.
class StructWellKnownType final : public WellKnownType {
 public:
  absl::StatusOr<absl::optional<int64_t>> FindFieldByName(
      absl::string_view name) const override {
    if (name == "fields") {
      return 1;
    }
    return absl::nullopt;
  }

  std::unique_ptr<WellKnownTypeValueBuilder> NewValueBuilder(
      ValueFactory& value_factory) const override {
    return std::make_unique<StructWellKnownTypeValueBuilder>(value_factory);
  }
};

// Global state for well known type handling to allow fast lookup.
struct WellKnownTypes {
  WellKnownTypes();

  BoolWellKnownType bool_wkt;
  Int32WellKnownType int32_wkt;
  Int64WellKnownType int64_wkt;
  UInt32WellKnownType uint32_wkt;
  UInt64WellKnownType uint64_wkt;
  FloatWellKnownType float_wkt;
  DoubleWellKnownType double_wkt;
  BytesWellKnownType bytes_wkt;
  StringWellKnownType string_wkt;
  DurationWellKnownType duration_wkt;
  TimestampWellKnownType timestamp_wkt;
  AnyWellKnownType any_wkt;
  ValueWellKnownType value_wkt;
  ListWellKnownType list_wkt;
  StructWellKnownType struct_wkt;
  absl::flat_hash_map<absl::string_view, const WellKnownType*> by_name;

  static const WellKnownTypes& Get();
};

const WellKnownTypes& WellKnownTypes::Get() {
  static const cel::internal::NoDestructor<WellKnownTypes> well_known_types;
  return well_known_types.get();
}

WellKnownTypes::WellKnownTypes() {
  by_name.reserve(15);
  by_name.insert_or_assign("google.protobuf.BoolValue", &bool_wkt);
  by_name.insert_or_assign("google.protobuf.Int32Value", &int32_wkt);
  by_name.insert_or_assign("google.protobuf.Int64Value", &int64_wkt);
  by_name.insert_or_assign("google.protobuf.UInt32Value", &uint32_wkt);
  by_name.insert_or_assign("google.protobuf.UInt64Value", &uint64_wkt);
  by_name.insert_or_assign("google.protobuf.FloatValue", &float_wkt);
  by_name.insert_or_assign("google.protobuf.DoubleValue", &double_wkt);
  by_name.insert_or_assign("google.protobuf.BytesValue", &bytes_wkt);
  by_name.insert_or_assign("google.protobuf.StringValue", &string_wkt);
  by_name.insert_or_assign("google.protobuf.Duration", &duration_wkt);
  by_name.insert_or_assign("google.protobuf.Timestamp", &timestamp_wkt);
  by_name.insert_or_assign("google.protobuf.Any", &any_wkt);
  by_name.insert_or_assign("google.protobuf.Value", &value_wkt);
  by_name.insert_or_assign("google.protobuf.ListValue", &list_wkt);
  by_name.insert_or_assign("google.protobuf.Struct", &struct_wkt);
}

// Like `StructType::FieldId` except that it owns the name instead of a view.
// This is to avoid awkward ownership issues during migration. In future we can
// consider just using a view.
using StructFieldId = absl::variant<std::string, int64_t>;

// `CreateStruct` implementation for message/struct.
class CreateStructStepForStruct final : public ExpressionStepBase {
 public:
  CreateStructStepForStruct(int64_t expr_id, Handle<StructType> type,
                            std::vector<StructFieldId> entries)
      : ExpressionStepBase(expr_id),
        type_(std::move(type)),
        entries_(std::move(entries)) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  absl::StatusOr<Handle<Value>> DoEvaluate(ExecutionFrame* frame) const;

  static absl::Status SetField(StructValueBuilderInterface& builder,
                               const StructFieldId& id, Handle<Value> value);

  Handle<StructType> type_;
  std::vector<StructFieldId> entries_;
};

// `CreateStruct` implementation for well known types.
class CreateStructStepForWellKnownType final : public ExpressionStepBase {
 public:
  CreateStructStepForWellKnownType(int64_t expr_id, const WellKnownType* type,
                                   std::vector<int64_t> entries)
      : ExpressionStepBase(expr_id),
        type_(std::move(type)),
        entries_(std::move(entries)) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  absl::StatusOr<Handle<Value>> DoEvaluate(ExecutionFrame* frame) const;

  const WellKnownType* type_;
  std::vector<int64_t> entries_;
};

// `CreateStruct` implementation for map.
class CreateStructStepForMap final : public ExpressionStepBase {
 public:
  CreateStructStepForMap(int64_t expr_id, size_t entry_count)
      : ExpressionStepBase(expr_id), entry_count_(entry_count) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  absl::StatusOr<Handle<Value>> DoEvaluate(ExecutionFrame* frame) const;

  size_t entry_count_;
};

absl::StatusOr<Handle<Value>> CreateStructStepForStruct::DoEvaluate(
    ExecutionFrame* frame) const {
  int entries_size = entries_.size();

  auto args = frame->value_stack().GetSpan(entries_size);

  if (frame->enable_unknowns()) {
    absl::optional<Handle<UnknownValue>> unknown_set =
        frame->attribute_utility().IdentifyAndMergeUnknowns(
            args, frame->value_stack().GetAttributeSpan(entries_size),
            /*use_partial=*/true);
    if (unknown_set.has_value()) {
      return *unknown_set;
    }
  }

  CEL_ASSIGN_OR_RETURN(auto builder,
                       type_->NewValueBuilder(frame->value_factory()));

  int index = 0;
  for (const auto& entry : entries_) {
    CEL_RETURN_IF_ERROR(SetField(*builder, entry, std::move(args[index++])));
  }
  return std::move(*builder).Build();
}

absl::Status CreateStructStepForStruct::SetField(
    StructValueBuilderInterface& builder, const StructFieldId& id,
    Handle<Value> value) {
  return absl::visit(
      cel::internal::Overloaded{
          [&builder, &value](absl::string_view name) mutable -> absl::Status {
            return builder.SetFieldByName(name, std::move(value));
          },
          [&builder, &value](int64_t number) mutable -> absl::Status {
            return builder.SetFieldByNumber(number, std::move(value));
          }},
      id);
}

absl::Status CreateStructStepForStruct::Evaluate(ExecutionFrame* frame) const {
  if (frame->value_stack().size() < entries_.size()) {
    return absl::InternalError("CreateStructStepForStruct: stack underflow");
  }

  Handle<Value> result;
  auto status_or_result = DoEvaluate(frame);
  if (status_or_result.ok()) {
    result = std::move(status_or_result).value();
  } else {
    result = frame->value_factory().CreateErrorValue(status_or_result.status());
  }
  frame->value_stack().Pop(entries_.size());
  frame->value_stack().Push(std::move(result));

  return absl::OkStatus();
}

absl::StatusOr<Handle<Value>> CreateStructStepForWellKnownType::DoEvaluate(
    ExecutionFrame* frame) const {
  int entries_size = entries_.size();

  auto args = frame->value_stack().GetSpan(entries_size);

  if (frame->enable_unknowns()) {
    absl::optional<Handle<UnknownValue>> unknown_set =
        frame->attribute_utility().IdentifyAndMergeUnknowns(
            args, frame->value_stack().GetAttributeSpan(entries_size),
            /*use_partial=*/true);
    if (unknown_set.has_value()) {
      return *unknown_set;
    }
  }

  auto builder = type_->NewValueBuilder(frame->value_factory());

  int index = 0;
  for (const auto& entry : entries_) {
    CEL_RETURN_IF_ERROR(
        builder->SetFieldByNumber(entry, std::move(args[index++])));
  }

  return std::move(*builder).Build();
}

absl::Status CreateStructStepForWellKnownType::Evaluate(
    ExecutionFrame* frame) const {
  if (frame->value_stack().size() < entries_.size()) {
    return absl::InternalError(
        "CreateStructStepForWellKnownType: stack underflow");
  }

  Handle<Value> result;
  auto status_or_result = DoEvaluate(frame);
  if (status_or_result.ok()) {
    result = std::move(status_or_result).value();
  } else {
    result = frame->value_factory().CreateErrorValue(status_or_result.status());
  }
  frame->value_stack().Pop(entries_.size());
  frame->value_stack().Push(std::move(result));

  return absl::OkStatus();
}

absl::StatusOr<Handle<Value>> CreateStructStepForMap::DoEvaluate(
    ExecutionFrame* frame) const {
  auto args = frame->value_stack().GetSpan(2 * entry_count_);

  if (frame->enable_unknowns()) {
    absl::optional<Handle<UnknownValue>> unknown_set =
        frame->attribute_utility().IdentifyAndMergeUnknowns(
            args, frame->value_stack().GetAttributeSpan(args.size()), true);
    if (unknown_set.has_value()) {
      return *unknown_set;
    }
  }

  cel::MapValueBuilder<Value, Value> map_builder(
      frame->value_factory(), frame->type_factory().GetDynType(),
      frame->type_factory().GetDynType());

  for (size_t i = 0; i < entry_count_; i += 1) {
    int map_key_index = 2 * i;
    int map_value_index = map_key_index + 1;
    CEL_RETURN_IF_ERROR(MapValue::CheckKey(*args[map_key_index]));
    auto key_status =
        map_builder.Put(args[map_key_index], args[map_value_index]);
    if (!key_status.ok()) {
      return CreateErrorValueFromView(google::protobuf::Arena::Create<absl::Status>(
          cel::extensions::ProtoMemoryManager::CastToProtoArena(
              frame->memory_manager()),
          key_status));
    }
  }

  return std::move(map_builder).Build();
}

absl::Status CreateStructStepForMap::Evaluate(ExecutionFrame* frame) const {
  if (frame->value_stack().size() < 2 * entry_count_) {
    return absl::InternalError("CreateStructStepForMap: stack underflow");
  }

  CEL_ASSIGN_OR_RETURN(auto result, DoEvaluate(frame));

  frame->value_stack().Pop(2 * entry_count_);
  frame->value_stack().Push(std::move(result));

  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateCreateStructStepForStruct(
    const cel::ast_internal::CreateStruct& create_struct_expr,
    absl::string_view type_name, Handle<Type> type, int64_t expr_id,
    TypeManager& type_manager) {
  // The resolved type should either be a struct or one of the well known types.
  if ((type)->Is<StructType>()) {
    // We resolved to a struct type. Use it.
    std::vector<StructFieldId> entries;
    entries.reserve(create_struct_expr.entries().size());
    for (const auto& entry : create_struct_expr.entries()) {
      CEL_ASSIGN_OR_RETURN(auto field, type.As<StructType>()->FindFieldByName(
                                           type_manager, entry.field_key()));
      if (!field.has_value()) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Invalid message creation: field '", entry.field_key(),
            "' not found in '", create_struct_expr.message_name(), "'"));
      }
      if (field->number != 0) {
        entries.push_back(field->number);
      } else {
        entries.emplace_back(std::string(field->name));
      }
    }
    return std::make_unique<CreateStructStepForStruct>(
        expr_id, std::move(type).As<StructType>(), std::move(entries));
  }
  // We resolved to something other than struct. Should be one of the well known
  // types.
  const auto& well_known_types = WellKnownTypes::Get();
  if (auto it = well_known_types.by_name.find(type_name);
      it != well_known_types.by_name.end()) {
    std::vector<int64_t> entries;
    entries.reserve(create_struct_expr.entries().size());
    for (const auto& entry : create_struct_expr.entries()) {
      CEL_ASSIGN_OR_RETURN(auto field,
                           it->second->FindFieldByName(entry.field_key()));
      if (!field.has_value()) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Invalid message creation: field '", entry.field_key(),
            "' not found in '", create_struct_expr.message_name(), "'"));
      }
      entries.push_back(*field);
    }
    return std::make_unique<CreateStructStepForWellKnownType>(
        expr_id, it->second, std::move(entries));
  }
  return absl::InvalidArgumentError(absl::StrCat(
      "Invalid struct creation: '", create_struct_expr.message_name(),
      "' is not a struct type"));
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateCreateStructStepForMap(
    const cel::ast_internal::CreateStruct& create_struct_expr,
    int64_t expr_id) {
  // Make map-creating step.
  return std::make_unique<CreateStructStepForMap>(
      expr_id, create_struct_expr.entries().size());
}

}  // namespace google::api::expr::runtime
