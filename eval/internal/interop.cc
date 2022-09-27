// Copyright 2022 Google LLC
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

#include "eval/internal/interop.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "base/value.h"
#include "base/values/list_value.h"
#include "base/values/map_value.h"
#include "eval/public/unknown_set.h"
#include "internal/status_macros.h"

namespace cel::interop_internal {

namespace {

using google::api::expr::runtime::CelList;
using google::api::expr::runtime::CelMap;
using google::api::expr::runtime::CelValue;
using google::api::expr::runtime::UnknownSet;

class LegacyListValue final : public ListValue {
 public:
  LegacyListValue(Persistent<ListType> type, const CelList* impl)
      : ListValue(std::move(type)), impl_(impl) {}

  size_t size() const override { return impl_->size(); }

  bool empty() const override { return impl_->empty(); }

  absl::StatusOr<Persistent<Value>> Get(ValueFactory& value_factory,
                                        size_t index) const override;

  std::string DebugString() const override;

  const CelList* value() const { return impl_; }

 private:
  const CelList* impl_;

  bool Equals(const Value& other) const override;
  void HashValue(absl::HashState state) const override;

  CEL_DECLARE_LIST_VALUE(LegacyListValue);
};

CEL_IMPLEMENT_LIST_VALUE(LegacyListValue);

absl::StatusOr<Persistent<Value>> LegacyListValue::Get(
    ValueFactory& value_factory, size_t index) const {
  return FromLegacyValue(value_factory, (*impl_)[index]);
}

std::string LegacyListValue::DebugString() const {
  // TODO(issues/5): maybe implement this
  return "<legacy>";
}

bool LegacyListValue::Equals(const Value& other) const {
  // TODO(issues/5): deal with this
  return false;
}

void LegacyListValue::HashValue(absl::HashState state) const {
  // TODO(issues/5): deal with this
}

class LegacyCelList final : public CelList {
 public:
  LegacyCelList(ValueFactory& value_factory, Persistent<ListValue> impl)
      : value_factory_(value_factory), impl_(std::move(impl)) {}

  CelValue operator[](int index) const override {
    auto value = impl_->Get(value_factory_, static_cast<size_t>(index));
    if (!value.ok()) {
      return CelValue::CreateError(value_factory_.memory_manager()
                                       .New<absl::Status>(value.status())
                                       .release());
    }
    auto legacy_value = ToLegacyValue(value_factory_, *value);
    if (!legacy_value.ok()) {
      return CelValue::CreateError(value_factory_.memory_manager()
                                       .New<absl::Status>(legacy_value.status())
                                       .release());
    }
    return std::move(legacy_value).value();
  }

  // List size
  int size() const override { return static_cast<int>(impl_->size()); }

  Persistent<ListValue> value() const { return impl_; }

 private:
  internal::TypeInfo TypeId() const override {
    return internal::TypeId<LegacyCelList>();
  }

  ValueFactory& value_factory_;
  Persistent<ListValue> impl_;
};

class LegacyMapValue final : public MapValue {
 public:
  LegacyMapValue(Persistent<MapType> type, ValueFactory& value_factory,
                 const CelMap* impl)
      : MapValue(std::move(type)), value_factory_(value_factory), impl_(impl) {}

  std::string DebugString() const override {
    // TODO(issues/5): maybe implement this
    return "<legacy>";
  }

  size_t size() const override { return static_cast<size_t>(impl_->size()); }

  bool empty() const override { return impl_->empty(); }

  bool Equals(const Value& other) const override {
    // TODO(issues/5): deal with this
    return false;
  }

  void HashValue(absl::HashState state) const override {
    // TODO(issues/5): deal with this
  }

  absl::StatusOr<Persistent<Value>> Get(
      ValueFactory& value_factory,
      const Persistent<Value>& key) const override {
    CEL_ASSIGN_OR_RETURN(auto legacy_key, ToLegacyValue(value_factory, key));
    auto legacy_value = (*impl_)[legacy_key];
    if (!legacy_value.has_value()) {
      return Persistent<Value>();
    }
    return FromLegacyValue(value_factory, std::move(legacy_value).value());
  }

  absl::StatusOr<bool> Has(const Persistent<Value>& key) const override {
    CEL_ASSIGN_OR_RETURN(auto legacy_value, ToLegacyValue(value_factory_, key));
    return impl_->Has(legacy_value);
  }

  absl::StatusOr<Persistent<ListValue>> ListKeys(
      ValueFactory& value_factory) const override {
    CEL_ASSIGN_OR_RETURN(auto type,
                         value_factory.type_factory().CreateListType(
                             value_factory.type_factory().GetDynType()));
    CEL_ASSIGN_OR_RETURN(auto* keys, impl_->ListKeys());
    return value_factory.CreateListValue<LegacyListValue>(type, keys);
  }

  const CelMap* value() const { return impl_; }

 private:
  ValueFactory& value_factory_;
  const CelMap* impl_;

  CEL_DECLARE_MAP_VALUE(LegacyMapValue);
};

CEL_IMPLEMENT_MAP_VALUE(LegacyMapValue);

class LegacyCelMap final : public CelMap {
 public:
  LegacyCelMap(ValueFactory& value_factory, Persistent<MapValue> impl)
      : value_factory_(value_factory), impl_(std::move(impl)) {}

  absl::optional<CelValue> operator[](CelValue key) const override {
    auto modern_key = FromLegacyValue(value_factory_, key);
    if (!modern_key.ok()) {
      return CelValue::CreateError(value_factory_.memory_manager()
                                       .New<absl::Status>(modern_key.status())
                                       .release());
    }
    auto modern_value = impl_->Get(value_factory_, *modern_key);
    if (!modern_value.ok()) {
      return CelValue::CreateError(value_factory_.memory_manager()
                                       .New<absl::Status>(modern_value.status())
                                       .release());
    }
    if (!*modern_value) {
      return absl::nullopt;
    }
    auto legacy_value = ToLegacyValue(value_factory_, *modern_value);
    if (!legacy_value.ok()) {
      return CelValue::CreateError(value_factory_.memory_manager()
                                       .New<absl::Status>(legacy_value.status())
                                       .release());
    }
    return std::move(legacy_value).value();
  }

  absl::StatusOr<bool> Has(const CelValue& key) const override {
    CEL_ASSIGN_OR_RETURN(auto modern_key, FromLegacyValue(value_factory_, key));
    return impl_->Has(modern_key);
  }

  int size() const override { return static_cast<int>(impl_->size()); }

  bool empty() const override { return impl_->empty(); }

  absl::StatusOr<const CelList*> ListKeys() const override {
    CEL_ASSIGN_OR_RETURN(auto list_keys, impl_->ListKeys(value_factory_));
    CEL_ASSIGN_OR_RETURN(auto legacy_list_keys,
                         ToLegacyValue(value_factory_, list_keys));
    return legacy_list_keys.ListOrDie();
  }

  Persistent<MapValue> value() const { return impl_; }

 private:
  internal::TypeInfo TypeId() const override {
    return internal::TypeId<LegacyCelMap>();
  }

  ValueFactory& value_factory_;
  Persistent<MapValue> impl_;
};

}  // namespace

internal::TypeInfo CelListAccess::TypeId(const CelList& list) {
  return list.TypeId();
}

internal::TypeInfo CelMapAccess::TypeId(const CelMap& map) {
  return map.TypeId();
}

absl::StatusOr<Persistent<StringValue>> CreateStringValueFromView(
    cel::ValueFactory& value_factory, absl::string_view input) {
  return value_factory.CreateStringValueFromView(input);
}

absl::StatusOr<Persistent<BytesValue>> CreateBytesValueFromView(
    cel::ValueFactory& value_factory, absl::string_view input) {
  return value_factory.CreateBytesValueFromView(input);
}

base_internal::StringValueRep GetStringValueRep(
    const Persistent<StringValue>& value) {
  return value->rep();
}

base_internal::BytesValueRep GetBytesValueRep(
    const Persistent<BytesValue>& value) {
  return value->rep();
}

std::shared_ptr<base_internal::UnknownSetImpl> GetUnknownValueImpl(
    const Persistent<UnknownValue>& value) {
  return value->impl_;
}

std::shared_ptr<base_internal::UnknownSetImpl> GetUnknownSetImpl(
    const UnknownSet& unknown_set) {
  return unknown_set.impl_;
}

void SetUnknownValueImpl(Persistent<UnknownValue>& value,
                         std::shared_ptr<base_internal::UnknownSetImpl> impl) {
  value->impl_ = std::move(impl);
}

void SetUnknownSetImpl(google::api::expr::runtime::UnknownSet& unknown_set,
                       std::shared_ptr<base_internal::UnknownSetImpl> impl) {
  unknown_set.impl_ = std::move(impl);
}

absl::StatusOr<Persistent<Value>> FromLegacyValue(
    cel::ValueFactory& value_factory, const CelValue& legacy_value) {
  switch (legacy_value.type()) {
    case CelValue::Type::kNullType:
      return value_factory.GetNullValue();
    case CelValue::Type::kBool:
      return value_factory.CreateBoolValue(legacy_value.BoolOrDie());
    case CelValue::Type::kInt64:
      return value_factory.CreateIntValue(legacy_value.Int64OrDie());
    case CelValue::Type::kUint64:
      return value_factory.CreateUintValue(legacy_value.Uint64OrDie());
    case CelValue::Type::kDouble:
      return value_factory.CreateDoubleValue(legacy_value.DoubleOrDie());
    case CelValue::Type::kString:
      return CreateStringValueFromView(value_factory,
                                       legacy_value.StringOrDie().value());
    case CelValue::Type::kBytes:
      return CreateBytesValueFromView(value_factory,
                                      legacy_value.BytesOrDie().value());
    case CelValue::Type::kMessage:
      break;
    case CelValue::Type::kDuration:
      return value_factory.CreateDurationValue(legacy_value.DurationOrDie());
    case CelValue::Type::kTimestamp:
      return value_factory.CreateTimestampValue(legacy_value.TimestampOrDie());
    case CelValue::Type::kList: {
      if (CelListAccess::TypeId(*legacy_value.ListOrDie()) ==
          internal::TypeId<LegacyCelList>()) {
        // Fast path.
        return static_cast<const LegacyCelList*>(legacy_value.ListOrDie())
            ->value();
      }
      CEL_ASSIGN_OR_RETURN(auto type,
                           value_factory.type_factory().CreateListType(
                               value_factory.type_factory().GetDynType()));
      return value_factory.CreateListValue<LegacyListValue>(
          type, legacy_value.ListOrDie());
    }
    case CelValue::Type::kMap: {
      if (CelMapAccess::TypeId(*legacy_value.MapOrDie()) ==
          internal::TypeId<LegacyCelMap>()) {
        // Fast path.
        return static_cast<const LegacyCelMap*>(legacy_value.MapOrDie())
            ->value();
      }
      CEL_ASSIGN_OR_RETURN(auto type,
                           value_factory.type_factory().CreateMapType(
                               value_factory.type_factory().GetDynType(),
                               value_factory.type_factory().GetDynType()));
      return value_factory.CreateMapValue<LegacyMapValue>(
          type, value_factory, legacy_value.MapOrDie());
    } break;
    case CelValue::Type::kUnknownSet: {
      auto value = value_factory.CreateUnknownValue();
      SetUnknownValueImpl(value,
                          GetUnknownSetImpl(*legacy_value.UnknownSetOrDie()));
      return value;
    }
    case CelValue::Type::kCelType: {
      CEL_ASSIGN_OR_RETURN(auto type, value_factory.type_manager().ResolveType(
                                          legacy_value.CelTypeOrDie().value()));
      return value_factory.CreateTypeValue(type);
    }
    case CelValue::Type::kError:
      return value_factory.CreateErrorValue(*legacy_value.ErrorOrDie());
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

namespace {

struct BytesValueToLegacyVisitor final {
  MemoryManager& memory_manager;

  absl::StatusOr<CelValue> operator()(absl::string_view value) const {
    return CelValue::CreateBytesView(value);
  }

  absl::StatusOr<CelValue> operator()(const absl::Cord& value) const {
    return CelValue::CreateBytes(
        memory_manager.New<std::string>(static_cast<std::string>(value))
            .release());
  }
};

struct StringValueToLegacyVisitor final {
  MemoryManager& memory_manager;

  absl::StatusOr<CelValue> operator()(absl::string_view value) const {
    return CelValue::CreateStringView(value);
  }

  absl::StatusOr<CelValue> operator()(const absl::Cord& value) const {
    return CelValue::CreateString(
        memory_manager.New<std::string>(static_cast<std::string>(value))
            .release());
  }
};

}  // namespace

absl::StatusOr<CelValue> ToLegacyValue(cel::ValueFactory& value_factory,
                                       const Persistent<Value>& value) {
  switch (value->kind()) {
    case Kind::kNullType:
      return CelValue::CreateNull();
    case Kind::kError:
      break;
    case Kind::kDyn:
      break;
    case Kind::kAny:
      break;
    case Kind::kType:
      // Should be fine, so long as we are using an arena allocator.
      return CelValue::CreateCelType(CelValue::CelTypeHolder(
          value_factory.memory_manager()
              .New<std::string>(value.As<TypeValue>()->value()->name())
              .release()));
    case Kind::kBool:
      return CelValue::CreateBool(value.As<BoolValue>()->value());
    case Kind::kInt:
      return CelValue::CreateInt64(value.As<IntValue>()->value());
    case Kind::kUint:
      return CelValue::CreateUint64(value.As<UintValue>()->value());
    case Kind::kDouble:
      return CelValue::CreateDouble(value.As<DoubleValue>()->value());
    case Kind::kString:
      return absl::visit(
          StringValueToLegacyVisitor{value_factory.memory_manager()},
          GetStringValueRep(value.As<StringValue>()));
    case Kind::kBytes:
      return absl::visit(
          BytesValueToLegacyVisitor{value_factory.memory_manager()},
          GetBytesValueRep(value.As<BytesValue>()));
    case Kind::kEnum:
      break;
    case Kind::kDuration:
      return CelValue::CreateDuration(value.As<DurationValue>()->value());
    case Kind::kTimestamp:
      return CelValue::CreateTimestamp(value.As<TimestampValue>()->value());
    case Kind::kList: {
      if (value.Is<LegacyListValue>()) {
        // Fast path.
        return CelValue::CreateList(value.As<LegacyListValue>()->value());
      }
      return CelValue::CreateList(
          value_factory.memory_manager()
              .New<LegacyCelList>(value_factory,
                                  Persistent<ListValue>(value.As<ListValue>()))
              .release());
    }
    case Kind::kMap: {
      if (value.Is<LegacyMapValue>()) {
        // Fast path.
        return CelValue::CreateMap(value.As<LegacyMapValue>()->value());
      }
      return CelValue::CreateMap(
          value_factory.memory_manager()
              .New<LegacyCelMap>(value_factory,
                                 Persistent<MapValue>(value.As<MapValue>()))
              .release());
    }
    case Kind::kStruct:
      break;
    case Kind::kUnknown: {
      auto* legacy_value =
          value_factory.memory_manager().New<UnknownSet>().release();
      SetUnknownSetImpl(*legacy_value,
                        GetUnknownValueImpl(value.As<UnknownValue>()));
      return CelValue::CreateUnknownSet(legacy_value);
    }
    default:
      break;
  }
  return absl::UnimplementedError(
      absl::StrCat("conversion from cel::Value to CelValue for type ",
                   KindToString(value->kind()), " is not yet implemented"));
}

}  // namespace cel::interop_internal
