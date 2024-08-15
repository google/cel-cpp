// Copyright 2024 Google LLC
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

#include "common/legacy_value.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/struct.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/functional/overload.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "base/attribute.h"
#include "base/internal/message_wrapper.h"
#include "common/casting.h"
#include "common/internal/arena_string.h"
#include "common/json.h"
#include "common/kind.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/unknown.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "common/value_manager.h"
#include "common/values/legacy_type_reflector.h"
#include "common/values/legacy_value_manager.h"
#include "eval/internal/cel_value_equal.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/message_wrapper.h"
#include "eval/public/structs/legacy_type_adapter.h"
#include "eval/public/structs/legacy_type_info_apis.h"
#include "extensions/protobuf/internal/legacy_value.h"
#include "extensions/protobuf/internal/struct_lite.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/casts.h"
#include "internal/status_macros.h"
#include "internal/time.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"

#if (defined(__GNUC__) && !defined(__clang__)) || ABSL_HAVE_ATTRIBUTE(used)
#define CEL_ATTRIBUTE_USED __attribute__((used))
#else
#define CEL_ATTRIBUTE_USED
#endif

#if (defined(__GNUC__) && !defined(__clang__)) || \
    ABSL_HAVE_ATTRIBUTE(visibility)
#define CEL_ATTRIBUTE_DEFAULT_VISIBILITY __attribute__((visibility("default")))
#else
#define CEL_ATTRIBUTE_DEFAULT_VISIBILITY
#endif

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type-c-linkage"
#endif

namespace cel {

namespace {

using google::api::expr::runtime::CelList;
using google::api::expr::runtime::CelMap;
using google::api::expr::runtime::CelMapBuilder;
using google::api::expr::runtime::CelValue;
using google::api::expr::runtime::ContainerBackedListImpl;
using google::api::expr::runtime::LegacyTypeInfoApis;
using google::api::expr::runtime::MessageWrapper;

constexpr absl::string_view kNullTypeName = "null_type";
constexpr absl::string_view kBoolTypeName = "bool";
constexpr absl::string_view kInt64TypeName = "int";
constexpr absl::string_view kUInt64TypeName = "uint";
constexpr absl::string_view kDoubleTypeName = "double";
constexpr absl::string_view kStringTypeName = "string";
constexpr absl::string_view kBytesTypeName = "bytes";
constexpr absl::string_view kDurationTypeName = "google.protobuf.Duration";
constexpr absl::string_view kTimestampTypeName = "google.protobuf.Timestamp";
constexpr absl::string_view kListTypeName = "list";
constexpr absl::string_view kMapTypeName = "map";
constexpr absl::string_view kCelTypeTypeName = "type";

absl::Status InvalidMapKeyTypeError(ValueKind kind) {
  return absl::InvalidArgumentError(
      absl::StrCat("Invalid map key type: '", ValueKindToString(kind), "'"));
}

const CelList* AsCelList(uintptr_t impl) {
  return reinterpret_cast<const CelList*>(impl);
}

const CelMap* AsCelMap(uintptr_t impl) {
  return reinterpret_cast<const CelMap*>(impl);
}

MessageWrapper AsMessageWrapper(uintptr_t message_ptr, uintptr_t type_info) {
  if ((message_ptr & base_internal::kMessageWrapperTagMask) ==
      base_internal::kMessageWrapperTagMessageValue) {
    return MessageWrapper::Builder(
               static_cast<google::protobuf::Message*>(
                   reinterpret_cast<google::protobuf::MessageLite*>(
                       message_ptr & base_internal::kMessageWrapperPtrMask)))
        .Build(reinterpret_cast<const LegacyTypeInfoApis*>(type_info));
  } else {
    return MessageWrapper::Builder(
               reinterpret_cast<google::protobuf::MessageLite*>(message_ptr))
        .Build(reinterpret_cast<const LegacyTypeInfoApis*>(type_info));
  }
}

class CelListIterator final : public ValueIterator {
 public:
  CelListIterator(google::protobuf::Arena* arena, const CelList* cel_list)
      : arena_(arena), cel_list_(cel_list), size_(cel_list_->size()) {}

  bool HasNext() override { return index_ < size_; }

  absl::Status Next(ValueManager&, Value& result) override {
    if (!HasNext()) {
      return absl::FailedPreconditionError(
          "ValueIterator::Next() called when ValueIterator::HasNext() returns "
          "false");
    }
    auto cel_value = cel_list_->Get(arena_, index_++);
    CEL_RETURN_IF_ERROR(ModernValue(arena_, cel_value, result));
    return absl::OkStatus();
  }

 private:
  google::protobuf::Arena* const arena_;
  const CelList* const cel_list_;
  const int size_;
  int index_ = 0;
};

absl::StatusOr<Json> CelValueToJson(google::protobuf::Arena* arena, CelValue value);

absl::StatusOr<JsonString> CelValueToJsonString(CelValue value) {
  switch (value.type()) {
    case CelValue::Type::kString:
      return JsonString(value.StringOrDie().value());
    default:
      return TypeConversionError(KindToString(value.type()), "string")
          .NativeValue();
  }
}

absl::StatusOr<JsonArray> CelListToJsonArray(google::protobuf::Arena* arena,
                                             const CelList* list);

absl::StatusOr<JsonObject> CelMapToJsonObject(google::protobuf::Arena* arena,
                                              const CelMap* map);

absl::StatusOr<JsonObject> MessageWrapperToJsonObject(
    google::protobuf::Arena* arena, MessageWrapper message_wrapper);

absl::StatusOr<Json> CelValueToJson(google::protobuf::Arena* arena, CelValue value) {
  switch (value.type()) {
    case CelValue::Type::kNullType:
      return kJsonNull;
    case CelValue::Type::kBool:
      return value.BoolOrDie();
    case CelValue::Type::kInt64:
      return JsonInt(value.Int64OrDie());
    case CelValue::Type::kUint64:
      return JsonUint(value.Uint64OrDie());
    case CelValue::Type::kDouble:
      return value.DoubleOrDie();
    case CelValue::Type::kString:
      return JsonString(value.StringOrDie().value());
    case CelValue::Type::kBytes:
      return JsonBytes(value.BytesOrDie().value());
    case CelValue::Type::kMessage:
      return MessageWrapperToJsonObject(arena, value.MessageWrapperOrDie());
    case CelValue::Type::kDuration: {
      CEL_ASSIGN_OR_RETURN(
          auto json, internal::EncodeDurationToJson(value.DurationOrDie()));
      return JsonString(std::move(json));
    }
    case CelValue::Type::kTimestamp: {
      CEL_ASSIGN_OR_RETURN(
          auto json, internal::EncodeTimestampToJson(value.TimestampOrDie()));
      return JsonString(std::move(json));
    }
    case CelValue::Type::kList:
      return CelListToJsonArray(arena, value.ListOrDie());
    case CelValue::Type::kMap:
      return CelMapToJsonObject(arena, value.MapOrDie());
    case CelValue::Type::kUnknownSet:
      ABSL_FALLTHROUGH_INTENDED;
    case CelValue::Type::kCelType:
      ABSL_FALLTHROUGH_INTENDED;
    case CelValue::Type::kError:
      ABSL_FALLTHROUGH_INTENDED;
    default:
      return absl::FailedPreconditionError(absl::StrCat(
          CelValue::TypeName(value.type()), " is unserializable to JSON"));
  }
}

absl::StatusOr<JsonArray> CelListToJsonArray(google::protobuf::Arena* arena,
                                             const CelList* list) {
  JsonArrayBuilder builder;
  const auto size = static_cast<size_t>(list->size());
  builder.reserve(size);
  for (size_t index = 0; index < size; ++index) {
    CEL_ASSIGN_OR_RETURN(
        auto element,
        CelValueToJson(arena, list->Get(arena, static_cast<int>(index))));
    builder.push_back(std::move(element));
  }
  return std::move(builder).Build();
}

absl::StatusOr<JsonObject> CelMapToJsonObject(google::protobuf::Arena* arena,
                                              const CelMap* map) {
  JsonObjectBuilder builder;
  const auto size = static_cast<size_t>(map->size());
  builder.reserve(size);
  CEL_ASSIGN_OR_RETURN(const auto* keys_list, map->ListKeys(arena));
  for (size_t index = 0; index < size; ++index) {
    auto key = keys_list->Get(arena, static_cast<int>(index));
    auto value = map->Get(arena, key);
    if (!value.has_value()) {
      return absl::FailedPreconditionError(
          "ListKeys() returned key not present map");
    }
    CEL_ASSIGN_OR_RETURN(auto json_key, CelValueToJsonString(key));
    CEL_ASSIGN_OR_RETURN(auto json_value, CelValueToJson(arena, *value));
    if (!builder.insert(std::pair{std::move(json_key), std::move(json_value)})
             .second) {
      return absl::FailedPreconditionError(
          "duplicate keys encountered serializing map as JSON");
    }
  }
  return std::move(builder).Build();
}

absl::StatusOr<JsonObject> MessageWrapperToJsonObject(
    google::protobuf::Arena* arena, MessageWrapper message_wrapper) {
  JsonObjectBuilder builder;
  const auto* type_info = message_wrapper.legacy_type_info();
  const auto* access_apis = type_info->GetAccessApis(message_wrapper);
  if (access_apis == nullptr) {
    return absl::FailedPreconditionError(
        absl::StrCat("LegacyTypeAccessApis missing for type: ",
                     type_info->GetTypename(message_wrapper)));
  }
  auto field_names = access_apis->ListFields(message_wrapper);
  builder.reserve(field_names.size());
  for (const auto& field_name : field_names) {
    CEL_ASSIGN_OR_RETURN(
        auto field,
        access_apis->GetField(field_name, message_wrapper,
                              ProtoWrapperTypeOptions::kUnsetNull,
                              extensions::ProtoMemoryManagerRef(arena)));
    CEL_ASSIGN_OR_RETURN(auto json_field, CelValueToJson(arena, field));
    builder.insert_or_assign(JsonString(field_name), std::move(json_field));
  }
  return std::move(builder).Build();
}

class CelListImpl final : public CelList {
 public:
  CelListImpl() : CelListImpl(nullptr, 0) {}

  CelListImpl(const CelValue* elements, size_t elements_size)
      : elements_(elements), elements_size_(elements_size) {}

  CelValue operator[](int index) const override {
    ABSL_DCHECK_LT(index, size());
    return elements_[index];
  }

  int size() const override { return static_cast<int>(elements_size_); }

  bool empty() const override { return elements_size_ == 0; }

 private:
  const CelValue* const elements_;
  const size_t elements_size_;
};

class CelMapImpl final : public CelMap, public CelList {
 public:
  using Entry = std::pair<CelValue, CelValue>;

  // We sort in the following partitions in order: [bool, int, uint, string].
  // Among the partitions the values are sorted as if by `<`.
  static bool LessKey(const CelValue& lhs, const CelValue& rhs) {
    if (lhs.IsBool()) {
      if (rhs.IsBool()) {
        return lhs.BoolOrDie() < rhs.BoolOrDie();
      }
      return true;
    }
    if (lhs.IsInt64()) {
      if (rhs.IsInt64()) {
        return lhs.Int64OrDie() < rhs.Int64OrDie();
      }
      return !rhs.IsBool();
    }
    if (lhs.IsUint64()) {
      if (rhs.IsUint64()) {
        return lhs.Uint64OrDie() < rhs.Uint64OrDie();
      }
      return !(rhs.IsBool() || rhs.IsInt64());
    }
    if (lhs.IsString()) {
      if (rhs.IsString()) {
        return lhs.StringOrDie().value() < rhs.StringOrDie().value();
      }
      return !(rhs.IsBool() || rhs.IsInt64() || rhs.IsUint64());
    }
    ABSL_DLOG(FATAL) << "invalid map key";
    return false;
  }

  static bool EqualKey(const CelValue& lhs, const CelValue& rhs) {
    if (lhs.IsBool() && rhs.IsBool()) {
      return lhs.BoolOrDie() == rhs.BoolOrDie();
    }
    if (lhs.IsInt64() && rhs.IsInt64()) {
      return lhs.Int64OrDie() == rhs.Int64OrDie();
    }
    if (lhs.IsUint64() && rhs.IsUint64()) {
      return lhs.Uint64OrDie() == rhs.Uint64OrDie();
    }
    if (lhs.IsString() && rhs.IsString()) {
      return lhs.StringOrDie().value() == rhs.StringOrDie().value();
    }
    return false;
  }

  struct KeyCompare {
    using is_transparent = void;

    bool operator()(const CelValue& lhs, const CelValue& rhs) const {
      return LessKey(lhs, rhs);
    }

    bool operator()(const Entry& lhs, const Entry& rhs) const {
      return (*this)(lhs.first, rhs.first);
    }

    bool operator()(const CelValue& lhs, const Entry& rhs) const {
      return (*this)(lhs, rhs.first);
    }

    bool operator()(const Entry& lhs, const CelValue& rhs) const {
      return (*this)(lhs.first, rhs);
    }
  };

  CelMapImpl() : CelMapImpl(nullptr, 0) {}

  CelMapImpl(const Entry* entries, size_t entries_size)
      : entries_(entries), entries_size_(entries_size) {}

  absl::optional<CelValue> operator[](CelValue key) const override {
    if (key.IsError() || key.IsUnknownSet()) {
      return key;
    }
    auto it = std::lower_bound(begin(), end(), key, KeyCompare{});
    if (it == end() || !EqualKey(it->first, key)) {
      return absl::nullopt;
    }
    return it->second;
  }

  CelValue operator[](int index) const override {
    if (index >= size()) {
      return CelValue::CreateNull();
    }
    return entries_[index].first;
  }

  absl::StatusOr<bool> Has(const CelValue& key) const override {
    CEL_RETURN_IF_ERROR(CelValue::CheckMapKeyType(key));
    auto it = std::lower_bound(begin(), end(), key, KeyCompare{});
    return it != end() && EqualKey(it->first, key);
  }

  int size() const override { return entries_size_; }

  bool empty() const override { return entries_size_ == 0; }

  const Entry* begin() const { return entries_; }

  const Entry* end() const { return entries_ + entries_size_; }

  absl::StatusOr<const CelList*> ListKeys() const override { return this; }

 private:
  const Entry* const entries_;
  const size_t entries_size_;
};

class CelListValue final : public ContainerBackedListImpl {
 public:
  CelListValue(ListType type, std::vector<CelValue> elements)
      : ContainerBackedListImpl(std::move(elements)), type_(std::move(type)) {}

  const ListType& GetType() const { return type_; }

 private:
  cel::NativeTypeId GetNativeTypeId() const override {
    return cel::NativeTypeId::For<CelListValue>();
  }

  ListType type_;
};

class CelListValueBuilder final : public ListValueBuilder {
 public:
  CelListValueBuilder(ValueFactory& value_factory, google::protobuf::Arena* arena,
                      ListType type)
      : value_factory_(value_factory), arena_(arena), type_(std::move(type)) {}

  absl::Status Add(Value value) override {
    if (value.Is<ErrorValue>()) {
      return value.As<ErrorValue>().NativeValue();
    }
    CEL_ASSIGN_OR_RETURN(auto legacy_value, LegacyValue(arena_, value));
    elements_.push_back(legacy_value);
    return absl::OkStatus();
  }

  bool IsEmpty() const override { return elements_.empty(); }

  size_t Size() const override { return elements_.size(); }

  void Reserve(size_t capacity) override { elements_.reserve(capacity); }

  ListValue Build() && override {
    if (elements_.empty()) {
      return ListValue();
    }
    return common_internal::LegacyListValue{reinterpret_cast<uintptr_t>(
        static_cast<CelList*>(google::protobuf::Arena::Create<CelListValue>(
            arena_, std::move(type_), std::move(elements_))))};
  }

 private:
  ValueFactory& value_factory_;
  google::protobuf::Arena* arena_;
  std::vector<CelValue> elements_;
  ListType type_;
};

class CelMapValue final : public CelMapBuilder {
 public:
  explicit CelMapValue(MapType type)
      : CelMapBuilder(), type_(std::move(type)) {}

  const MapType& GetType() const { return type_; }

 private:
  cel::NativeTypeId GetNativeTypeId() const override {
    return cel::NativeTypeId::For<CelMapValue>();
  }

  MapType type_;
};

class CelMapValueBuilder final : public MapValueBuilder {
 public:
  explicit CelMapValueBuilder(google::protobuf::Arena* arena, MapType type)
      : arena_(arena),
        builder_(google::protobuf::Arena::Create<CelMapValue>(arena_, std::move(type))) {}

  absl::Status Put(Value key, Value value) override {
    if (key.Is<ErrorValue>()) {
      return key.As<ErrorValue>().NativeValue();
    }
    if (value.Is<ErrorValue>()) {
      return value.As<ErrorValue>().NativeValue();
    }
    CEL_ASSIGN_OR_RETURN(auto legacy_key, LegacyValue(arena_, key));
    CEL_ASSIGN_OR_RETURN(auto legacy_value, LegacyValue(arena_, value));
    return builder_->Add(legacy_key, legacy_value);
  }

  bool IsEmpty() const override { return builder_->size() == 0; }

  size_t Size() const override { return static_cast<size_t>(builder_->size()); }

  MapValue Build() && override {
    return common_internal::LegacyMapValue{
        reinterpret_cast<uintptr_t>(static_cast<CelMap*>(builder_))};
  }

 private:
  google::protobuf::Arena* arena_;
  CelMapValue* builder_;
};

}  // namespace

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY ListType
cel_common_internal_LegacyListValue_GetType(uintptr_t impl,
                                            TypeManager& type_manager) {
  const auto* cel_list = AsCelList(impl);
  if (NativeTypeId::Of(*cel_list) == NativeTypeId::For<CelListValue>()) {
    return cel::internal::down_cast<const CelListValue*>(cel_list)->GetType();
  }
  return ListType(type_manager.GetDynListType());
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY std::string
cel_common_internal_LegacyListValue_DebugString(uintptr_t impl) {
  return CelValue::CreateList(AsCelList(impl)).DebugString();
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY
    absl::StatusOr<size_t>
    cel_common_internal_LegacyListValue_GetSerializedSize(uintptr_t impl) {
  return absl::UnimplementedError("GetSerializedSize is not implemented");
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY absl::Status
cel_common_internal_LegacyListValue_SerializeTo(uintptr_t impl,
                                                absl::Cord& serialized_value) {
  google::protobuf::ListValue message;
  google::protobuf::Arena arena;
  CEL_ASSIGN_OR_RETURN(auto array, CelListToJsonArray(&arena, AsCelList(impl)));
  CEL_RETURN_IF_ERROR(
      extensions::protobuf_internal::GeneratedListValueProtoFromJson(array,
                                                                     message));
  if (!message.SerializePartialToCord(&serialized_value)) {
    return absl::UnknownError("failed to serialize google.protobuf.ListValue");
  }
  return absl::OkStatus();
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY
    absl::StatusOr<JsonArray>
    cel_common_internal_LegacyListValue_ConvertToJsonArray(uintptr_t impl) {
  google::protobuf::Arena arena;
  return CelListToJsonArray(&arena, AsCelList(impl));
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY bool
cel_common_internal_LegacyListValue_IsEmpty(uintptr_t impl) {
  return AsCelList(impl)->empty();
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY size_t
cel_common_internal_LegacyListValue_Size(uintptr_t impl) {
  return static_cast<size_t>(AsCelList(impl)->size());
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY absl::Status
cel_common_internal_LegacyListValue_Get(uintptr_t impl,
                                        ValueManager& value_manager,
                                        size_t index, Value& result) {
  auto* arena =
      extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager());
  if (ABSL_PREDICT_FALSE(index < 0 || index >= AsCelList(impl)->size())) {
    result = value_manager.CreateErrorValue(
        absl::InvalidArgumentError("index out of bounds"));
    return absl::OkStatus();
  }
  CEL_RETURN_IF_ERROR(ModernValue(
      arena, AsCelList(impl)->Get(arena, static_cast<int>(index)), result));
  return absl::OkStatus();
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY absl::Status
cel_common_internal_LegacyListValue_ForEach(
    uintptr_t impl, ValueManager& value_manager,
    ListValue::ForEachWithIndexCallback callback) {
  auto* arena =
      extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager());
  const auto size = AsCelList(impl)->size();
  Value element;
  for (int index = 0; index < size; ++index) {
    CEL_RETURN_IF_ERROR(
        ModernValue(arena, AsCelList(impl)->Get(arena, index), element));
    CEL_ASSIGN_OR_RETURN(auto ok, callback(index, Value(element)));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY
    absl::StatusOr<absl::Nonnull<ValueIteratorPtr>>
    cel_common_internal_LegacyListValue_NewIterator(
        uintptr_t impl, ValueManager& value_manager) {
  return std::make_unique<CelListIterator>(
      extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager()),
      AsCelList(impl));
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY absl::Status
cel_common_internal_LegacyListValue_Contains(uintptr_t impl,
                                             ValueManager& value_manager,
                                             const Value& other,
                                             Value& result) {
  auto* arena =
      extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager());
  CEL_ASSIGN_OR_RETURN(auto legacy_other, LegacyValue(arena, other));
  const auto* cel_list = AsCelList(impl);
  for (int i = 0; i < cel_list->size(); ++i) {
    auto element = cel_list->Get(arena, i);
    absl::optional<bool> equal =
        interop_internal::CelValueEqualImpl(element, legacy_other);
    // Heterogenous equality behavior is to just return false if equality
    // undefined.
    if (equal.has_value() && *equal) {
      result = BoolValue{true};
      return absl::OkStatus();
    }
  }
  result = BoolValue{false};
  return absl::OkStatus();
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY MapType
cel_common_internal_LegacyMapValue_GetType(uintptr_t impl,
                                           TypeManager& type_manager) {
  const auto* cel_map = AsCelMap(impl);
  if (NativeTypeId::Of(*cel_map) == NativeTypeId::For<CelMapValue>()) {
    return cel::internal::down_cast<const CelMapValue*>(cel_map)->GetType();
  }
  return MapType(type_manager.GetDynDynMapType());
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY std::string
cel_common_internal_LegacyMapValue_DebugString(uintptr_t impl) {
  return CelValue::CreateMap(AsCelMap(impl)).DebugString();
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY
    absl::StatusOr<size_t>
    cel_common_internal_LegacyMapValue_GetSerializedSize(uintptr_t impl) {
  return absl::UnimplementedError("GetSerializedSize is not implemented");
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY absl::Status
cel_common_internal_LegacyMapValue_SerializeTo(uintptr_t impl,
                                               absl::Cord& serialized_value) {
  google::protobuf::Struct message;
  google::protobuf::Arena arena;
  CEL_ASSIGN_OR_RETURN(auto object, CelMapToJsonObject(&arena, AsCelMap(impl)));
  CEL_RETURN_IF_ERROR(
      extensions::protobuf_internal::GeneratedStructProtoFromJson(object,
                                                                  message));
  if (!message.SerializePartialToCord(&serialized_value)) {
    return absl::UnknownError("failed to serialize google.protobuf.Struct");
  }
  return absl::OkStatus();
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY
    absl::StatusOr<JsonObject>
    cel_common_internal_LegacyMapValue_ConvertToJsonObject(uintptr_t impl) {
  google::protobuf::Arena arena;
  return CelMapToJsonObject(&arena, AsCelMap(impl));
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY bool
cel_common_internal_LegacyMapValue_IsEmpty(uintptr_t impl) {
  return AsCelMap(impl)->empty();
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY size_t
cel_common_internal_LegacyMapValue_Size(uintptr_t impl) {
  return static_cast<size_t>(AsCelMap(impl)->size());
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY
    absl::StatusOr<bool>
    cel_common_internal_LegacyMapValue_Find(uintptr_t impl,
                                            ValueManager& value_manager,
                                            const Value& key, Value& result) {
  switch (key.kind()) {
    case ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUnknown:
      result = Value{key};
      return false;
    case ValueKind::kBool:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kInt:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUint:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kString:
      break;
    default:
      return InvalidMapKeyTypeError(key.kind());
  }
  auto* arena =
      extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager());
  CEL_ASSIGN_OR_RETURN(auto cel_key, LegacyValue(arena, key));
  auto cel_value = AsCelMap(impl)->Get(arena, cel_key);
  if (!cel_value.has_value()) {
    result = NullValue{};
    return false;
  }
  CEL_RETURN_IF_ERROR(ModernValue(arena, *cel_value, result));
  return true;
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY absl::Status
cel_common_internal_LegacyMapValue_Get(uintptr_t impl,
                                       ValueManager& value_manager,
                                       const Value& key, Value& result) {
  switch (key.kind()) {
    case ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUnknown:
      result = Value{key};
      return absl::OkStatus();
    case ValueKind::kBool:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kInt:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUint:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kString:
      break;
    default:
      return InvalidMapKeyTypeError(key.kind());
  }
  auto* arena =
      extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager());
  CEL_ASSIGN_OR_RETURN(auto cel_key, LegacyValue(arena, key));
  auto cel_value = AsCelMap(impl)->Get(arena, cel_key);
  if (!cel_value.has_value()) {
    result = NoSuchKeyError(key.DebugString());
    return absl::OkStatus();
  }
  CEL_RETURN_IF_ERROR(ModernValue(arena, *cel_value, result));
  return absl::OkStatus();
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY absl::Status
cel_common_internal_LegacyMapValue_Has(uintptr_t impl,
                                       ValueManager& value_manager,
                                       const Value& key, Value& result) {
  switch (key.kind()) {
    case ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUnknown:
      result = Value{key};
      return absl::OkStatus();
    case ValueKind::kBool:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kInt:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUint:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kString:
      break;
    default:
      return InvalidMapKeyTypeError(key.kind());
  }
  auto* arena =
      extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager());
  CEL_ASSIGN_OR_RETURN(auto cel_key, LegacyValue(arena, key));
  CEL_ASSIGN_OR_RETURN(auto has, AsCelMap(impl)->Has(cel_key));
  result = BoolValue{has};
  return absl::OkStatus();
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY absl::Status
cel_common_internal_LegacyMapValue_ListKeys(uintptr_t impl,
                                            ValueManager& value_manager,
                                            ListValue& result) {
  auto* arena =
      extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager());
  CEL_ASSIGN_OR_RETURN(auto keys, AsCelMap(impl)->ListKeys(arena));
  result = ListValue{
      common_internal::LegacyListValue{reinterpret_cast<uintptr_t>(keys)}};
  return absl::OkStatus();
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY absl::Status
cel_common_internal_LegacyMapValue_ForEach(uintptr_t impl,
                                           ValueManager& value_manager,
                                           MapValue::ForEachCallback callback) {
  auto* arena =
      extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager());
  CEL_ASSIGN_OR_RETURN(auto keys, AsCelMap(impl)->ListKeys(arena));
  const auto size = keys->size();
  Value key;
  Value value;
  for (int index = 0; index < size; ++index) {
    auto cel_key = keys->Get(arena, index);
    auto cel_value = *AsCelMap(impl)->Get(arena, cel_key);
    CEL_RETURN_IF_ERROR(ModernValue(arena, cel_key, key));
    CEL_RETURN_IF_ERROR(ModernValue(arena, cel_value, value));
    CEL_ASSIGN_OR_RETURN(auto ok, callback(key, value));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY
    absl::StatusOr<absl::Nonnull<ValueIteratorPtr>>
    cel_common_internal_LegacyMapValue_NewIterator(
        uintptr_t impl, ValueManager& value_manager) {
  auto* arena =
      extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager());
  CEL_ASSIGN_OR_RETURN(auto keys, AsCelMap(impl)->ListKeys(arena));
  return cel_common_internal_LegacyListValue_NewIterator(
      reinterpret_cast<uintptr_t>(keys), value_manager);
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY std::string
cel_common_internal_LegacyStructValue_DebugString(uintptr_t message_ptr,
                                                  uintptr_t type_info) {
  auto message_wrapper = AsMessageWrapper(message_ptr, type_info);
  return message_wrapper.legacy_type_info()->DebugString(message_wrapper);
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY
    absl::StatusOr<size_t>
    cel_common_internal_LegacyStructValue_GetSerializedSize(
        uintptr_t message_ptr, uintptr_t type_info) {
  auto message_wrapper = AsMessageWrapper(message_ptr, type_info);
  if (ABSL_PREDICT_FALSE(message_wrapper.message_ptr() == nullptr)) {
    return 0;
  }
  return message_wrapper.message_ptr()->ByteSizeLong();
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY absl::Status
cel_common_internal_LegacyStructValue_SerializeTo(uintptr_t message_ptr,
                                                  uintptr_t type_info,
                                                  absl::Cord& value) {
  auto message_wrapper = AsMessageWrapper(message_ptr, type_info);
  if (ABSL_PREDICT_TRUE(
          message_wrapper.message_ptr()->SerializePartialToCord(&value))) {
    return absl::OkStatus();
  }
  return absl::UnknownError("failed to serialize protocol buffer message");
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY std::string
cel_common_internal_LegacyStructValue_GetType(uintptr_t message_ptr,
                                              uintptr_t type_info) {
  auto message_wrapper = AsMessageWrapper(message_ptr, type_info);
  if (ABSL_PREDICT_TRUE(message_wrapper.message_ptr() != nullptr)) {
    return message_wrapper.message_ptr()->GetTypeName();
  }
  return std::string(
      message_wrapper.legacy_type_info()->GetTypename(message_wrapper));
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY absl::string_view
cel_common_internal_LegacyStructValue_GetTypeName(uintptr_t message_ptr,
                                                  uintptr_t type_info) {
  auto message_wrapper = AsMessageWrapper(message_ptr, type_info);
  return message_wrapper.legacy_type_info()->GetTypename(message_wrapper);
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY
    absl::StatusOr<JsonObject>
    cel_common_internal_LegacyStructValue_ConvertToJsonObject(
        uintptr_t message_ptr, uintptr_t type_info) {
  google::protobuf::Arena arena;
  return MessageWrapperToJsonObject(&arena,
                                    AsMessageWrapper(message_ptr, type_info));
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY absl::Status
cel_common_internal_LegacyStructValue_GetFieldByName(
    uintptr_t message_ptr, uintptr_t type_info, ValueManager& value_manager,
    absl::string_view name, Value& result,
    ProtoWrapperTypeOptions unboxing_options) {
  auto message_wrapper = AsMessageWrapper(message_ptr, type_info);
  const auto* access_apis =
      message_wrapper.legacy_type_info()->GetAccessApis(message_wrapper);
  if (ABSL_PREDICT_FALSE(access_apis == nullptr)) {
    result = NoSuchFieldError(name);
    return absl::OkStatus();
  }
  CEL_ASSIGN_OR_RETURN(
      auto cel_value,
      access_apis->GetField(name, message_wrapper, unboxing_options,
                            value_manager.GetMemoryManager()));
  CEL_RETURN_IF_ERROR(ModernValue(
      extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager()),
      cel_value, result));
  return absl::OkStatus();
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY absl::Status
cel_common_internal_LegacyStructValue_GetFieldByNumber(
    uintptr_t, uintptr_t, ValueManager&, int64_t, Value&,
    ProtoWrapperTypeOptions) {
  return absl::UnimplementedError(
      "access to fields by numbers is not available for legacy structs");
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY
    absl::StatusOr<bool>
    cel_common_internal_LegacyStructValue_HasFieldByName(
        uintptr_t message_ptr, uintptr_t type_info, absl::string_view name) {
  auto message_wrapper = AsMessageWrapper(message_ptr, type_info);
  const auto* access_apis =
      message_wrapper.legacy_type_info()->GetAccessApis(message_wrapper);
  if (ABSL_PREDICT_FALSE(access_apis == nullptr)) {
    return NoSuchFieldError(name).NativeValue();
  }
  return access_apis->HasField(name, message_wrapper);
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY
    absl::StatusOr<bool>
    cel_common_internal_LegacyStructValue_HasFieldByNumber(uintptr_t, uintptr_t,
                                                           int64_t) {
  return absl::UnimplementedError(
      "access to fields by numbers is not available for legacy structs");
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY absl::Status
cel_common_internal_LegacyStructValue_Equal(uintptr_t message_ptr,
                                            uintptr_t type_info,
                                            ValueManager& value_manager,
                                            const Value& other, Value& result) {
  if (auto legacy_struct_value = As<common_internal::LegacyStructValue>(other);
      legacy_struct_value.has_value()) {
    auto message_wrapper = AsMessageWrapper(message_ptr, type_info);
    const auto* access_apis =
        message_wrapper.legacy_type_info()->GetAccessApis(message_wrapper);
    if (ABSL_PREDICT_FALSE(access_apis == nullptr)) {
      return absl::UnimplementedError(
          absl::StrCat("legacy access APIs missing for ",
                       cel_common_internal_LegacyStructValue_GetTypeName(
                           message_ptr, type_info)));
    }
    auto other_message_wrapper =
        AsMessageWrapper(legacy_struct_value->message_ptr(),
                         legacy_struct_value->legacy_type_info());
    result = BoolValue{
        access_apis->IsEqualTo(message_wrapper, other_message_wrapper)};
    return absl::OkStatus();
  }
  if (auto struct_value = As<StructValue>(other); struct_value.has_value()) {
    return common_internal::StructValueEqual(
        value_manager,
        common_internal::LegacyStructValue(message_ptr, type_info),
        *struct_value, result);
  }
  result = BoolValue{false};
  return absl::OkStatus();
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY bool
cel_common_internal_LegacyStructValue_IsZeroValue(uintptr_t message_ptr,
                                                  uintptr_t type_info) {
  auto message_wrapper = AsMessageWrapper(message_ptr, type_info);
  const auto* access_apis =
      message_wrapper.legacy_type_info()->GetAccessApis(message_wrapper);
  if (ABSL_PREDICT_FALSE(access_apis == nullptr)) {
    return false;
  }
  return access_apis->ListFields(message_wrapper).empty();
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY absl::Status
cel_common_internal_LegacyStructValue_ForEachField(
    uintptr_t message_ptr, uintptr_t type_info, ValueManager& value_manager,
    StructValue::ForEachFieldCallback callback) {
  auto message_wrapper = AsMessageWrapper(message_ptr, type_info);
  const auto* access_apis =
      message_wrapper.legacy_type_info()->GetAccessApis(message_wrapper);
  if (ABSL_PREDICT_FALSE(access_apis == nullptr)) {
    return absl::UnimplementedError(
        absl::StrCat("legacy access APIs missing for ",
                     cel_common_internal_LegacyStructValue_GetTypeName(
                         message_ptr, type_info)));
  }
  auto field_names = access_apis->ListFields(message_wrapper);
  Value value;
  for (const auto& field_name : field_names) {
    CEL_ASSIGN_OR_RETURN(
        auto cel_value,
        access_apis->GetField(field_name, message_wrapper,
                              ProtoWrapperTypeOptions::kUnsetNull,
                              value_manager.GetMemoryManager()));
    CEL_RETURN_IF_ERROR(ModernValue(
        extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager()),
        cel_value, value));
    CEL_ASSIGN_OR_RETURN(auto ok, callback(field_name, value));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY
    absl::StatusOr<int>
    cel_common_internal_LegacyStructValue_Qualify(
        uintptr_t message_ptr, uintptr_t type_info, ValueManager& value_manager,
        absl::Span<const SelectQualifier> qualifiers, bool presence_test,
        Value& result) {
  if (ABSL_PREDICT_FALSE(qualifiers.empty())) {
    return absl::InvalidArgumentError("invalid select qualifier path.");
  }
  auto message_wrapper = AsMessageWrapper(message_ptr, type_info);
  const auto* access_apis =
      message_wrapper.legacy_type_info()->GetAccessApis(message_wrapper);
  if (ABSL_PREDICT_FALSE(access_apis == nullptr)) {
    absl::string_view field_name = absl::visit(
        absl::Overload(
            [](const FieldSpecifier& field) -> absl::string_view {
              return field.name;
            },
            [](const AttributeQualifier& field) -> absl::string_view {
              return field.GetStringKey().value_or("<invalid field>");
            }),
        qualifiers.front());
    result = NoSuchFieldError(field_name);
    return -1;
  }
  CEL_ASSIGN_OR_RETURN(
      auto legacy_result,
      access_apis->Qualify(qualifiers, message_wrapper, presence_test,
                           value_manager.GetMemoryManager()));
  CEL_RETURN_IF_ERROR(ModernValue(
      extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager()),
      legacy_result.value, result));
  return legacy_result.qualifier_count;
}

namespace {}  // namespace

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY
    absl::StatusOr<Unique<ListValueBuilder>>
    cel_common_internal_LegacyTypeReflector_NewListValueBuilder(
        ValueFactory& value_factory, const ListType& type) {
  auto memory_manager = value_factory.GetMemoryManager();
  auto* arena = extensions::ProtoMemoryManagerArena(memory_manager);
  if (arena == nullptr) {
    return absl::UnimplementedError(
        "cel_common_internal_LegacyTypeReflector_NewListValueBuilder only "
        "supports google::protobuf::Arena");
  }
  return memory_manager.MakeUnique<CelListValueBuilder>(value_factory, arena,
                                                        ListType(type));
}

extern "C" CEL_ATTRIBUTE_USED CEL_ATTRIBUTE_DEFAULT_VISIBILITY
    absl::StatusOr<Unique<MapValueBuilder>>
    cel_common_internal_LegacyTypeReflector_NewMapValueBuilder(
        ValueFactory& value_factory, const MapType& type) {
  auto memory_manager = value_factory.GetMemoryManager();
  auto* arena = extensions::ProtoMemoryManagerArena(memory_manager);
  if (arena == nullptr) {
    return absl::UnimplementedError(
        "cel_common_internal_LegacyTypeReflector_NewMapValueBuilder only "
        "supports google::protobuf::Arena");
  }
  return memory_manager.MakeUnique<CelMapValueBuilder>(arena, MapType(type));
}

absl::Status ModernValue(google::protobuf::Arena* arena,
                         google::api::expr::runtime::CelValue legacy_value,
                         Value& result) {
  switch (legacy_value.type()) {
    case CelValue::Type::kNullType:
      result = NullValue{};
      return absl::OkStatus();
    case CelValue::Type::kBool:
      result = BoolValue{legacy_value.BoolOrDie()};
      return absl::OkStatus();
    case CelValue::Type::kInt64:
      result = IntValue{legacy_value.Int64OrDie()};
      return absl::OkStatus();
    case CelValue::Type::kUint64:
      result = UintValue{legacy_value.Uint64OrDie()};
      return absl::OkStatus();
    case CelValue::Type::kDouble:
      result = DoubleValue{legacy_value.DoubleOrDie()};
      return absl::OkStatus();
    case CelValue::Type::kString:
      result = StringValue{
          common_internal::ArenaString(legacy_value.StringOrDie().value())};
      return absl::OkStatus();
    case CelValue::Type::kBytes:
      result = BytesValue{
          common_internal::ArenaString(legacy_value.BytesOrDie().value())};
      return absl::OkStatus();
    case CelValue::Type::kMessage: {
      auto message_wrapper = legacy_value.MessageWrapperOrDie();
      result = common_internal::LegacyStructValue{
          reinterpret_cast<uintptr_t>(message_wrapper.message_ptr()) |
              (message_wrapper.HasFullProto()
                   ? base_internal::kMessageWrapperTagMessageValue
                   : uintptr_t{0}),
          reinterpret_cast<uintptr_t>(message_wrapper.legacy_type_info())};
      return absl::OkStatus();
    }
    case CelValue::Type::kDuration:
      result = DurationValue{legacy_value.DurationOrDie()};
      return absl::OkStatus();
    case CelValue::Type::kTimestamp:
      result = TimestampValue{legacy_value.TimestampOrDie()};
      return absl::OkStatus();
    case CelValue::Type::kList:
      result = ListValue{common_internal::LegacyListValue{
          reinterpret_cast<uintptr_t>(legacy_value.ListOrDie())}};
      return absl::OkStatus();
    case CelValue::Type::kMap:
      result = MapValue{common_internal::LegacyMapValue{
          reinterpret_cast<uintptr_t>(legacy_value.MapOrDie())}};
      return absl::OkStatus();
    case CelValue::Type::kUnknownSet:
      result = UnknownValue{*legacy_value.UnknownSetOrDie()};
      return absl::OkStatus();
    case CelValue::Type::kCelType: {
      auto type_name = legacy_value.CelTypeOrDie().value();
      if (type_name == kNullTypeName) {
        result = TypeValue{NullType{}};
        return absl::OkStatus();
      }
      if (type_name == kBoolTypeName) {
        result = TypeValue{BoolType{}};
        return absl::OkStatus();
      }
      if (type_name == kInt64TypeName) {
        result = TypeValue{IntType{}};
        return absl::OkStatus();
      }
      if (type_name == kUInt64TypeName) {
        result = TypeValue{UintType{}};
        return absl::OkStatus();
      }
      if (type_name == kDoubleTypeName) {
        result = TypeValue{DoubleType{}};
        return absl::OkStatus();
      }
      if (type_name == kStringTypeName) {
        result = TypeValue{StringType{}};
        return absl::OkStatus();
      }
      if (type_name == kBytesTypeName) {
        result = TypeValue{BytesType{}};
        return absl::OkStatus();
      }
      if (type_name == kDurationTypeName) {
        result = TypeValue{DurationType{}};
        return absl::OkStatus();
      }
      if (type_name == kTimestampTypeName) {
        result = TypeValue{TimestampType{}};
        return absl::OkStatus();
      }
      if (type_name == kListTypeName) {
        result = TypeValue{ListType{}};
        return absl::OkStatus();
      }
      if (type_name == kMapTypeName) {
        result = TypeValue{MapType{}};
        return absl::OkStatus();
      }
      if (type_name == kCelTypeTypeName) {
        result = TypeValue{TypeType{}};
        return absl::OkStatus();
      }
      result = TypeValue{common_internal::MakeBasicStructType(type_name)};
      return absl::OkStatus();
    }
    case CelValue::Type::kError:
      result = ErrorValue{*legacy_value.ErrorOrDie()};
      return absl::OkStatus();
    case CelValue::Type::kAny:
      return absl::InternalError(absl::StrCat(
          "illegal attempt to convert special CelValue type ",
          CelValue::TypeName(legacy_value.type()), " to cel::Value"));
    default:
      break;
  }
  return absl::InvalidArgumentError(absl::StrCat(
      "cel::Value does not support ", KindToString(legacy_value.type())));
}

absl::StatusOr<google::api::expr::runtime::CelValue> LegacyValue(
    google::protobuf::Arena* arena, const Value& modern_value) {
  switch (modern_value.kind()) {
    case ValueKind::kNull:
      return CelValue::CreateNull();
    case ValueKind::kBool:
      return CelValue::CreateBool(Cast<BoolValue>(modern_value).NativeValue());
    case ValueKind::kInt:
      return CelValue::CreateInt64(Cast<IntValue>(modern_value).NativeValue());
    case ValueKind::kUint:
      return CelValue::CreateUint64(
          Cast<UintValue>(modern_value).NativeValue());
    case ValueKind::kDouble:
      return CelValue::CreateDouble(
          Cast<DoubleValue>(modern_value).NativeValue());
    case ValueKind::kString: {
      const auto& string_value = Cast<StringValue>(modern_value);
      if (common_internal::AsSharedByteString(string_value).IsPooledString()) {
        return CelValue::CreateStringView(
            common_internal::AsSharedByteString(string_value).AsStringView());
      }
      return string_value.NativeValue(absl::Overload(
          [arena](absl::string_view string) -> CelValue {
            return CelValue::CreateString(
                google::protobuf::Arena::Create<std::string>(arena, string));
          },
          [arena](const absl::Cord& string) -> CelValue {
            return CelValue::CreateString(google::protobuf::Arena::Create<std::string>(
                arena, static_cast<std::string>(string)));
          }));
    }
    case ValueKind::kBytes: {
      const auto& bytes_value = Cast<BytesValue>(modern_value);
      if (common_internal::AsSharedByteString(bytes_value).IsPooledString()) {
        return CelValue::CreateBytesView(
            common_internal::AsSharedByteString(bytes_value).AsStringView());
      }
      return bytes_value.NativeValue(absl::Overload(
          [arena](absl::string_view string) -> CelValue {
            return CelValue::CreateBytes(
                google::protobuf::Arena::Create<std::string>(arena, string));
          },
          [arena](const absl::Cord& string) -> CelValue {
            return CelValue::CreateBytes(google::protobuf::Arena::Create<std::string>(
                arena, static_cast<std::string>(string)));
          }));
    }
    case ValueKind::kStruct: {
      if (auto legacy_struct_value =
              As<common_internal::LegacyStructValue>(modern_value);
          legacy_struct_value.has_value()) {
        return CelValue::CreateMessageWrapper(
            AsMessageWrapper(legacy_struct_value->message_ptr(),
                             legacy_struct_value->legacy_type_info()));
      }
      CEL_ASSIGN_OR_RETURN(
          auto message_wrapper,
          extensions::protobuf_internal::MessageWrapperFromValue(
              Value(modern_value), arena));
      return CelValue::CreateMessageWrapper(message_wrapper);
    }
    case ValueKind::kDuration:
      return CelValue::CreateUncheckedDuration(
          Cast<DurationValue>(modern_value).NativeValue());
    case ValueKind::kTimestamp:
      return CelValue::CreateTimestamp(
          Cast<TimestampValue>(modern_value).NativeValue());
    case ValueKind::kList: {
      if (auto legacy_list_value =
              As<common_internal::LegacyListValue>(modern_value);
          legacy_list_value.has_value()) {
        return CelValue::CreateList(
            AsCelList(legacy_list_value->NativeValue()));
      }
      // We have a non-legacy `ListValue`. We are going to have to
      // materialize it in its entirety to `CelList`.
      auto list_value = Cast<ListValue>(modern_value);
      CEL_ASSIGN_OR_RETURN(auto list_value_size, list_value.Size());
      if (list_value_size == 0) {
        return CelValue::CreateList();
      }
      auto* elements = static_cast<CelValue*>(arena->AllocateAligned(
          sizeof(CelValue) * list_value_size, alignof(CelValue)));
      common_internal::LegacyTypeReflector value_provider;
      common_internal::LegacyValueManager value_manager(
          extensions::ProtoMemoryManagerRef(arena), value_provider);
      CEL_RETURN_IF_ERROR(list_value.ForEach(
          value_manager,
          [arena, elements](size_t index,
                            const Value& element) -> absl::StatusOr<bool> {
            CEL_ASSIGN_OR_RETURN(elements[index], LegacyValue(arena, element));
            return true;
          }));
      // We don't bother registering CelListImpl's destructor, it doesn't own
      // anything and is not trivially destructible simply due to having a
      // virtual destructor.
      return CelValue::CreateList(::new (
          arena->AllocateAligned(sizeof(CelListImpl), alignof(CelListImpl)))
                                      CelListImpl(elements, list_value_size));
    }
    case ValueKind::kMap: {
      if (auto legacy_map_value =
              As<common_internal::LegacyMapValue>(modern_value);
          legacy_map_value.has_value()) {
        return CelValue::CreateMap(AsCelMap(legacy_map_value->NativeValue()));
      }
      // We have a non-legacy `MapValue`. We are going to have to
      // materialize it in its entirety to `CelMap`.
      auto map_value = Cast<MapValue>(modern_value);
      CEL_ASSIGN_OR_RETURN(auto map_value_size, map_value.Size());
      if (map_value_size == 0) {
        return CelValue::CreateMap();
      }
      auto* entries = static_cast<CelMapImpl::Entry*>(
          arena->AllocateAligned(sizeof(CelMapImpl::Entry) * map_value_size,
                                 alignof(CelMapImpl::Entry)));
      size_t entry_count = 0;
      common_internal::LegacyTypeReflector value_provider;
      common_internal::LegacyValueManager value_manager(
          extensions::ProtoMemoryManagerRef(arena), value_provider);
      CEL_RETURN_IF_ERROR(map_value.ForEach(
          value_manager,
          [arena, entries, &entry_count](
              const Value& key, const Value& value) -> absl::StatusOr<bool> {
            auto* entry = ::new (static_cast<void*>(entries + entry_count++))
                CelMapImpl::Entry{};
            CEL_ASSIGN_OR_RETURN(entry->first, LegacyValue(arena, key));
            CEL_ASSIGN_OR_RETURN(entry->second, LegacyValue(arena, value));
            return true;
          }));
      std::sort(entries, entries + map_value_size, CelMapImpl::KeyCompare{});
      return CelValue::CreateMap(::new (
          arena->AllocateAligned(sizeof(CelMapImpl), alignof(CelMapImpl)))
                                     CelMapImpl(entries, map_value_size));
    }
    case ValueKind::kUnknown:
      return CelValue::CreateUnknownSet(google::protobuf::Arena::Create<Unknown>(
          arena, Cast<UnknownValue>(modern_value).NativeValue()));
    case ValueKind::kType:
      return CelValue::CreateCelType(
          CelValue::CelTypeHolder(google::protobuf::Arena::Create<std::string>(
              arena, Cast<TypeValue>(modern_value).NativeValue().name())));
    case ValueKind::kError:
      return CelValue::CreateError(google::protobuf::Arena::Create<absl::Status>(
          arena, Cast<ErrorValue>(modern_value).NativeValue()));
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("google::api::expr::runtime::CelValue does not support ",
                       ValueKindToString(modern_value.kind())));
  }
}

namespace interop_internal {

absl::StatusOr<Value> FromLegacyValue(google::protobuf::Arena* arena,
                                      const CelValue& legacy_value, bool) {
  switch (legacy_value.type()) {
    case CelValue::Type::kNullType:
      return NullValue{};
    case CelValue::Type::kBool:
      return BoolValue(legacy_value.BoolOrDie());
    case CelValue::Type::kInt64:
      return IntValue(legacy_value.Int64OrDie());
    case CelValue::Type::kUint64:
      return UintValue(legacy_value.Uint64OrDie());
    case CelValue::Type::kDouble:
      return DoubleValue(legacy_value.DoubleOrDie());
    case CelValue::Type::kString:
      return StringValue(
          common_internal::ArenaString(legacy_value.StringOrDie().value()));
    case CelValue::Type::kBytes:
      return BytesValue(
          common_internal::ArenaString(legacy_value.BytesOrDie().value()));
    case CelValue::Type::kMessage: {
      auto message_wrapper = legacy_value.MessageWrapperOrDie();
      return common_internal::LegacyStructValue{
          reinterpret_cast<uintptr_t>(message_wrapper.message_ptr()) |
              (message_wrapper.HasFullProto()
                   ? base_internal::kMessageWrapperTagMessageValue
                   : uintptr_t{0}),
          reinterpret_cast<uintptr_t>(message_wrapper.legacy_type_info())};
    }
    case CelValue::Type::kDuration:
      return DurationValue(legacy_value.DurationOrDie());
    case CelValue::Type::kTimestamp:
      return TimestampValue(legacy_value.TimestampOrDie());
    case CelValue::Type::kList:
      return ListValue{common_internal::LegacyListValue{
          reinterpret_cast<uintptr_t>(legacy_value.ListOrDie())}};
    case CelValue::Type::kMap:
      return MapValue{common_internal::LegacyMapValue{
          reinterpret_cast<uintptr_t>(legacy_value.MapOrDie())}};
    case CelValue::Type::kUnknownSet:
      return UnknownValue{*legacy_value.UnknownSetOrDie()};
    case CelValue::Type::kCelType:
      return CreateTypeValueFromView(arena,
                                     legacy_value.CelTypeOrDie().value());
    case CelValue::Type::kError:
      return ErrorValue(*legacy_value.ErrorOrDie());
    case CelValue::Type::kAny:
      return absl::InternalError(absl::StrCat(
          "illegal attempt to convert special CelValue type ",
          CelValue::TypeName(legacy_value.type()), " to cel::Value"));
    default:
      break;
  }
  return absl::UnimplementedError(absl::StrCat(
      "conversion from CelValue to cel::Value for type ",
      CelValue::TypeName(legacy_value.type()), " is not yet implemented"));
}

absl::StatusOr<google::api::expr::runtime::CelValue> ToLegacyValue(
    google::protobuf::Arena* arena, const Value& value, bool) {
  switch (value.kind()) {
    case ValueKind::kNull:
      return CelValue::CreateNull();
    case ValueKind::kBool:
      return CelValue::CreateBool(Cast<BoolValue>(value).NativeValue());
    case ValueKind::kInt:
      return CelValue::CreateInt64(Cast<IntValue>(value).NativeValue());
    case ValueKind::kUint:
      return CelValue::CreateUint64(Cast<UintValue>(value).NativeValue());
    case ValueKind::kDouble:
      return CelValue::CreateDouble(Cast<DoubleValue>(value).NativeValue());
    case ValueKind::kString: {
      const auto& string_value = Cast<StringValue>(value);
      if (common_internal::AsSharedByteString(string_value).IsPooledString()) {
        return CelValue::CreateStringView(
            common_internal::AsSharedByteString(string_value).AsStringView());
      }
      return string_value.NativeValue(absl::Overload(
          [arena](absl::string_view string) -> CelValue {
            return CelValue::CreateString(
                google::protobuf::Arena::Create<std::string>(arena, string));
          },
          [arena](const absl::Cord& string) -> CelValue {
            return CelValue::CreateString(google::protobuf::Arena::Create<std::string>(
                arena, static_cast<std::string>(string)));
          }));
    }
    case ValueKind::kBytes: {
      const auto& bytes_value = Cast<BytesValue>(value);
      if (common_internal::AsSharedByteString(bytes_value).IsPooledString()) {
        return CelValue::CreateBytesView(
            common_internal::AsSharedByteString(bytes_value).AsStringView());
      }
      return bytes_value.NativeValue(absl::Overload(
          [arena](absl::string_view string) -> CelValue {
            return CelValue::CreateBytes(
                google::protobuf::Arena::Create<std::string>(arena, string));
          },
          [arena](const absl::Cord& string) -> CelValue {
            return CelValue::CreateBytes(google::protobuf::Arena::Create<std::string>(
                arena, static_cast<std::string>(string)));
          }));
    }
    case ValueKind::kStruct: {
      if (auto legacy_struct_value =
              As<common_internal::LegacyStructValue>(value);
          legacy_struct_value.has_value()) {
        return CelValue::CreateMessageWrapper(
            AsMessageWrapper(legacy_struct_value->message_ptr(),
                             legacy_struct_value->legacy_type_info()));
      }
      CEL_ASSIGN_OR_RETURN(
          auto message_wrapper,
          extensions::protobuf_internal::MessageWrapperFromValue(value, arena));
      return CelValue::CreateMessageWrapper(message_wrapper);
    }
    case ValueKind::kDuration:
      return CelValue::CreateUncheckedDuration(
          Cast<DurationValue>(value).NativeValue());
    case ValueKind::kTimestamp:
      return CelValue::CreateTimestamp(
          Cast<TimestampValue>(value).NativeValue());
    case ValueKind::kList: {
      if (auto legacy_list_value = As<common_internal::LegacyListValue>(value);
          legacy_list_value.has_value()) {
        return CelValue::CreateList(
            AsCelList(legacy_list_value->NativeValue()));
      }
      // We have a non-legacy `ListValue`. We are going to have to
      // materialize it in its entirety to `CelList`.
      auto list_value = Cast<ListValue>(value);
      CEL_ASSIGN_OR_RETURN(auto list_value_size, list_value.Size());
      if (list_value_size == 0) {
        return CelValue::CreateList();
      }
      auto* elements = static_cast<CelValue*>(arena->AllocateAligned(
          sizeof(CelValue) * list_value_size, alignof(CelValue)));
      common_internal::LegacyTypeReflector value_provider;
      common_internal::LegacyValueManager value_manager(
          extensions::ProtoMemoryManagerRef(arena), value_provider);
      CEL_RETURN_IF_ERROR(list_value.ForEach(
          value_manager,
          [arena, elements](size_t index,
                            const Value& element) -> absl::StatusOr<bool> {
            CEL_ASSIGN_OR_RETURN(elements[index], LegacyValue(arena, element));
            return true;
          }));
      // We don't bother registering CelListImpl's destructor, it doesn't own
      // anything and is not trivially destructible simply due to having a
      // virtual destructor.
      return CelValue::CreateList(::new (
          arena->AllocateAligned(sizeof(CelListImpl), alignof(CelListImpl)))
                                      CelListImpl(elements, list_value_size));
    }
    case ValueKind::kMap: {
      if (auto legacy_map_value = As<common_internal::LegacyMapValue>(value);
          legacy_map_value.has_value()) {
        return CelValue::CreateMap(AsCelMap(legacy_map_value->NativeValue()));
      }
      // We have a non-legacy `MapValue`. We are going to have to
      // materialize it in its entirety to `CelMap`.
      auto map_value = Cast<MapValue>(value);
      CEL_ASSIGN_OR_RETURN(auto map_value_size, map_value.Size());
      if (map_value_size == 0) {
        return CelValue::CreateMap();
      }
      auto* entries = static_cast<CelMapImpl::Entry*>(
          arena->AllocateAligned(sizeof(CelMapImpl::Entry) * map_value_size,
                                 alignof(CelMapImpl::Entry)));
      size_t entry_count = 0;
      common_internal::LegacyTypeReflector value_provider;
      common_internal::LegacyValueManager value_manager(
          extensions::ProtoMemoryManagerRef(arena), value_provider);
      CEL_RETURN_IF_ERROR(map_value.ForEach(
          value_manager,
          [arena, entries, &entry_count](
              const Value& key, const Value& value) -> absl::StatusOr<bool> {
            auto* entry = ::new (static_cast<void*>(entries + entry_count++))
                CelMapImpl::Entry{};
            CEL_ASSIGN_OR_RETURN(entry->first, LegacyValue(arena, key));
            CEL_ASSIGN_OR_RETURN(entry->second, LegacyValue(arena, value));
            return true;
          }));
      std::sort(entries, entries + map_value_size, CelMapImpl::KeyCompare{});
      return CelValue::CreateMap(::new (
          arena->AllocateAligned(sizeof(CelMapImpl), alignof(CelMapImpl)))
                                     CelMapImpl(entries, map_value_size));
    }
    case ValueKind::kUnknown:
      return CelValue::CreateUnknownSet(google::protobuf::Arena::Create<Unknown>(
          arena, Cast<UnknownValue>(value).NativeValue()));
    case ValueKind::kType:
      return CelValue::CreateCelType(
          CelValue::CelTypeHolder(google::protobuf::Arena::Create<std::string>(
              arena, Cast<TypeValue>(value).NativeValue().name())));
    case ValueKind::kError:
      return CelValue::CreateError(google::protobuf::Arena::Create<absl::Status>(
          arena, Cast<ErrorValue>(value).NativeValue()));
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("google::api::expr::runtime::CelValue does not support ",
                       ValueKindToString(value.kind())));
  }
}

Value LegacyValueToModernValueOrDie(
    google::protobuf::Arena* arena, const google::api::expr::runtime::CelValue& value,
    bool unchecked) {
  auto status_or_value = FromLegacyValue(arena, value, unchecked);
  ABSL_CHECK_OK(status_or_value.status());  // Crash OK
  return std::move(*status_or_value);
}

std::vector<Value> LegacyValueToModernValueOrDie(
    google::protobuf::Arena* arena,
    absl::Span<const google::api::expr::runtime::CelValue> values,
    bool unchecked) {
  std::vector<Value> modern_values;
  modern_values.reserve(values.size());
  for (const auto& value : values) {
    modern_values.push_back(
        LegacyValueToModernValueOrDie(arena, value, unchecked));
  }
  return modern_values;
}

google::api::expr::runtime::CelValue ModernValueToLegacyValueOrDie(
    google::protobuf::Arena* arena, const Value& value, bool unchecked) {
  auto status_or_value = ToLegacyValue(arena, value, unchecked);
  ABSL_CHECK_OK(status_or_value.status());  // Crash OK
  return std::move(*status_or_value);
}

TypeValue CreateTypeValueFromView(google::protobuf::Arena* arena,
                                  absl::string_view input) {
  auto type_name = input;
  if (type_name == kNullTypeName) {
    return TypeValue{NullType{}};
  }
  if (type_name == kBoolTypeName) {
    return TypeValue{BoolType{}};
  }
  if (type_name == kInt64TypeName) {
    return TypeValue{IntType{}};
  }
  if (type_name == kUInt64TypeName) {
    return TypeValue{UintType{}};
  }
  if (type_name == kDoubleTypeName) {
    return TypeValue{DoubleType{}};
  }
  if (type_name == kStringTypeName) {
    return TypeValue{StringType{}};
  }
  if (type_name == kBytesTypeName) {
    return TypeValue{BytesType{}};
  }
  if (type_name == kDurationTypeName) {
    return TypeValue{DurationType{}};
  }
  if (type_name == kTimestampTypeName) {
    return TypeValue{TimestampType{}};
  }
  if (type_name == kListTypeName) {
    return TypeValue{ListType{}};
  }
  if (type_name == kMapTypeName) {
    return TypeValue{MapType{}};
  }
  if (type_name == kCelTypeTypeName) {
    return TypeValue{TypeType{}};
  }
  // This is bad, but technically OK since we are using an arena.
  return TypeValue{common_internal::MakeBasicStructType(type_name)};
}

bool TestOnly_IsLegacyListBuilder(const ListValueBuilder& builder) {
  return dynamic_cast<const CelListValueBuilder*>(&builder) != nullptr;
}

bool TestOnly_IsLegacyMapBuilder(const MapValueBuilder& builder) {
  return dynamic_cast<const CelMapValueBuilder*>(&builder) != nullptr;
}

}  // namespace interop_internal

}  // namespace cel

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
