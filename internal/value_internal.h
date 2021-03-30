// Internal declarations for value.h/.cc.

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_VALUE_INTERNAL_H_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_VALUE_INTERNAL_H_H_

#include <iosfwd>
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "common/enum.h"
#include "common/error.h"
#include "common/id.h"
#include "common/parent_ref.h"
#include "common/type.h"
#include "common/unknown.h"
#include "internal/adapter_util.h"
#include "internal/cast.h"
#include "internal/ref_countable.h"

namespace google {
namespace api {
namespace expr {

namespace common {

class List;
class Map;
class Object;

}  // namespace common

namespace internal {

// The types stored directly in ValueData.
using InlineTypes = types<std::nullptr_t, bool, int64_t, uint64_t, double,
                          common::NamedEnumValue, common::BasicType,
                          common::ObjectType, common::EnumType>;
// The types stored by reffed copy in ValueData.
using RefCopyTypes = types<absl::Duration, absl::Time, common::Error,
                           common::UnnamedEnumValue, common::UnrecognizedType>;

template <typename T>
using is_shared_value = std::is_convertible<T*, const common::SharedValue*>;

// If a type can be returned by pointer or reference.
template <typename T>
using is_direct_type =
    or_t<type_in<T, InlineTypes>, type_in<T, RefCopyTypes>, is_shared_value<T>>;

// If a type can be stored in a 'Value'.
template <typename T>
using is_value_type = or_t<is_direct_type<T>, is_numeric<T>, is_string<T>>;

class BaseValue {
 protected:
  // The return type of `Value::get_if` call for T.
  template <typename T>
  using GetIfType = absl::conditional_t<is_direct_type<T>::value, const T*,
                                        absl::optional<T>>;

  // The return type of `Value::get` call for T.
  template <typename T>
  using GetType = absl::conditional_t<is_direct_type<T>::value, const T&, T>;

  // If the string is 8 bytes, it is assumed to be copy-on-write and is stored
  // inline. Otherwise it is held in a ref-counted container.
  using OwnedStr = SizeLimitHolder<std::string>;

  // Holds the string_view and parent in a refcounted container.
  using ParentOwnedStr =
      Holder<absl::string_view, Ref<ParentOwned<common::ValueRef, Copy>>>;

  // Holds the string_view in a ref counted container.
  using UnownedStr = RefCopyHolder<absl::string_view>;

  // The variant type used by `expr::Value`.
  using ValueData = absl::variant<
      // Small types can be stored inline.
      CopyHolder<std::nullptr_t>,          // null_value
      CopyHolder<bool>,                    // bool_value
      CopyHolder<int64_t>,                 // int_value
      CopyHolder<uint64_t>,                // uint_value
      CopyHolder<double>,                  // double_value
      CopyHolder<common::NamedEnumValue>,  // enum_value
      CopyHolder<common::BasicType>,       // type_value
      CopyHolder<common::ObjectType>,      // type_value
      CopyHolder<common::EnumType>,        // type_value
      CopyHolder<common::Id>,              // unknown of a single id.

      // Other types are stored as unowned, owned pointers.
      // An intrusive shared pointer is used to minimize the variant size.
      OwnedStr, ParentOwnedStr, UnownedStr,  // string_value
      OwnedStr, ParentOwnedStr, UnownedStr,  // bytes_value
      RefPtrHolder<common::Map>,
      UnownedPtrHolder<const common::Map>,  // map_value
      RefPtrHolder<common::List>,
      UnownedPtrHolder<const common::List>,  // list_value
      RefPtrHolder<common::Object>,
      UnownedPtrHolder<const common::Object>,   // object_value
      RefCopyHolder<absl::Duration>,            // duration
      RefCopyHolder<absl::Time>,                // time
      RefCopyHolder<common::UnnamedEnumValue>,  // enum_value
      RefCopyHolder<common::UnrecognizedType>,  // type_value
      RefCopyHolder<common::Error>,             // error
      RefCopyHolder<common::Unknown>>;          // unknown

  // The public types associated with each Value::Kind entry.
  using KindToType = types<std::nullptr_t,     // kNull
                           bool,               // kBool
                           int64_t,            // kInt
                           uint64_t,           // kUInt
                           double,             // kDouble
                           absl::string_view,  // kString
                           absl::string_view,  // kBytes
                           common::Type,       // kType
                           common::Map,        // kMap
                           common::List,       // kList
                           common::Object,     // kObject
                           common::EnumValue,  // kEnum
                           absl::Duration,     // kDuration
                           absl::Time,         // kTime
                           common::Error,      // kError
                           common::Unknown>;   // kUnknown

  enum Index {
    kNull,        // null_value
    kBool,        // bool_value
    kInt,         // int_value
    kUInt,        // uint_value
    kDouble,      // double_value
    kNamedEnum,   // enum_value
    kBasicType,   // type_value
    kObjectType,  // type_value
    kEnumType,    // type_value
    kId,          // unknown
    kInlineEnd,

    kStr = kInlineEnd,  // string_value
    kStrView,           // string_value
    kStrPtr,            // string_value
    kBytes,             // bytes_value
    kBytesView,         // bytes_value
    kBytesPtr,          // bytes_value
    kMap,               // map_value
    kMapPtr,            // map_value
    kList,              // list_value
    kListPtr,           // list_value
    kObject,            // object_value
    kObjectPtr,         // object_value
    kOptionalOwnershipEnd,

    kDuration = kOptionalOwnershipEnd,  // duration
    kTime,                              // time
    kObjectEnd,
    kUnnamedEnum = kObjectEnd,  // enum_value
    kUnrecognizedType,          // type_value
    kValueEnd,

    kError = kValueEnd,  // error
    kUnknown,            // unknown
    DATA_SIZE,
  };

  /**
   * A helper function to grab either the unowned or owned string value.
   *
   * Assumes the owned value is immediately after the unowned value.
   *
   * @tparam I the index of the unowned value.
   */
  template <std::size_t I>
  static absl::string_view GetStr(const ValueData& data);

  BaseValue() = default;
  ~BaseValue() = default;

 private:
  friend class ValueVisitTest;
  friend class ValueAdapterTest;

  template <typename T>
  using NumericValueType = absl::conditional_t<
      is_int<T>::value, int64_t,
      absl::conditional_t<is_uint<T>::value, uint64_t, double>>;
  template <typename T>
  using CustomValueType = absl::conditional_t<
      std::is_convertible<T*, const common::Map*>::value, common::Map,
      absl::conditional_t<std::is_convertible<T*, const common::List*>::value,
                          common::List, common::Object>>;

  template <typename T>
  using HolderType = absl::conditional_t<type_in<T, InlineTypes>::value,
                                         CopyHolder<T>, RefCopyHolder<T>>;

  // A ValueData visitor that, for any type in 'Alts', returns the associated
  // value as T.
  template <typename Fallback, typename T, typename... Alts>
  using GetVisitor = OrderedVisitor<
      ConvertAdapter<StrictHolderAdapter<HolderAdapter, Alts...>, T>, Fallback>;

  template <typename Fallback, typename T>
  using GetPtrVisitor =
      OrderedVisitor<StrictHolderAdapter<HolderPtrAdapter, T>, Fallback>;

  // Base class for types that are stored as one of several other types.
  template <typename T, typename F = T, typename... Alts>
  struct BaseTypeHelper {
    static absl::optional<T> get_if(const ValueData* data);
    static T get(const ValueData& data);
  };

  /**
   * An adapter that converts private Value internal types into public types.
   */
  struct ValueDataAdapter {
    // Convert the single id case into Unknown.
    common::Unknown operator()(const common::Id& value) {
      return common::Unknown(value);
    }

    // Normalize string values to string_view
    template <typename T>
    specialize_ift<is_string<T>, absl::string_view> operator()(T& value);
  };

 protected:
  using ValueAdapter = CompositeAdapter<HolderAdapter, ValueDataAdapter>;

  /** The adapted visitor type. */
  template <typename Visitor>
  using AdaptedVisitor = VisitorAdapter<Visitor, ValueAdapter>;

  /**
   * The result of applying an adapted visitor to Value.
   *
   * Values must contain only Value types.
   */
  template <typename Visitor, typename... Values>
  using VisitType =
      decltype(absl::visit(inst_of<AdaptedVisitor<Visitor>&&>(),
                           inst_of<const ValueData&, Values>()...));

  // A collection of helper functions for the given type.
  template <typename T, typename = specialize>
  struct TypeHelper : BaseTypeHelper<T> {};
};

template <std::size_t I>
absl::string_view BaseValue::GetStr(const ValueData& data) {
  switch (data.index()) {
    case I:  // Owned
      return *absl::get<I>(data);
    case I + 1:  // Parent owned
      return *absl::get<I + 1>(data);
    default:  // Unowned
      return *absl::get<I + 2>(data);
  }
}

// Base class for types that are stored directly.
template <typename T>
struct BaseValue::BaseTypeHelper<T, T> {
  static const T* get_if(const ValueData* data);
  static const T& get(const ValueData& data);
};

// Base class for types that are stored as a runtime compatible type.
template <typename T, typename N>
struct BaseValue::BaseTypeHelper<T, N> {
  static absl::optional<T> get_if(const ValueData* data);
  static T get(const ValueData& data);
};

// Base class for SharedValue types.
template <typename T>
struct BaseValue::BaseTypeHelper<T, common::SharedValue> {
  using C = CustomValueType<T>;
  static const T* get_if(const ValueData* data);
  static const T& get(const ValueData& data);
};

// Specialization for CustomValues.
template <typename T>
struct BaseValue::TypeHelper<T, specialize_ift<is_shared_value<T>>>
    : BaseTypeHelper<T, common::SharedValue> {};

// Specialization for numeric types.
template <typename T>
struct BaseValue::TypeHelper<T, specialize_ift<is_numeric<T>>>
    : BaseTypeHelper<T, NumericValueType<T>> {};

template <>
struct BaseValue::TypeHelper<common::Unknown, specialize>
    : BaseTypeHelper<common::Unknown, common::Unknown, common::Id> {};

template <>
struct BaseValue::TypeHelper<common::EnumValue, specialize>
    : BaseTypeHelper<common::EnumValue, common::UnnamedEnumValue,
                     common::NamedEnumValue> {};

template <>
struct BaseValue::TypeHelper<common::Type, specialize>
    : BaseTypeHelper<common::Type, common::BasicType, common::EnumType,
                     common::ObjectType, common::UnrecognizedType> {};
template <typename T>
specialize_ift<is_string<T>, absl::string_view> BaseValue::ValueDataAdapter::
operator()(T& value) {
  return value;
}

template <typename T, typename F, typename... Alts>
absl::optional<T> BaseValue::BaseTypeHelper<T, F, Alts...>::get_if(
    const ValueData* data) {
  using R = absl::optional<T>;
  return absl::visit(GetVisitor<DefaultVisitor<R>, R, F, Alts...>(), *data);
}

template <typename T, typename F, typename... Alts>
T BaseValue::BaseTypeHelper<T, F, Alts...>::get(const ValueData& data) {
  return absl::visit(GetVisitor<BadAccessVisitor<T>, T, F, Alts...>(), data);
}

template <typename T>
const T* BaseValue::BaseTypeHelper<T, T>::get_if(const ValueData* data) {
  if (auto holder = absl::get_if<HolderType<T>>(data)) {
    return &holder->value();
  }
  return nullptr;
}

template <typename T>
const T& BaseValue::BaseTypeHelper<T, T>::get(const ValueData& data) {
  return *absl::get<HolderType<T>>(data);
}

template <typename T, typename N>
absl::optional<T> BaseValue::BaseTypeHelper<T, N>::get_if(
    const ValueData* data) {
  auto value = BaseTypeHelper<N>::get_if(data);
  if (!value || !representable_as<T>(*value)) {
    return absl::nullopt;
  }
  return absl::optional<T>(absl::in_place, *value);
}

template <typename T, typename N>
T BaseValue::BaseTypeHelper<T, N>::get(const ValueData& data) {
  const N& value = BaseTypeHelper<N>::get(data);
  if (!representable_as<T>(value)) {
    // Throw bad_variant_access without using `throw` keyword.
    return absl::get<0>(absl::variant<T, int>(absl::in_place_index<1>, 1));
  }
  return T(value);
}

template <typename T>
const T* BaseValue::BaseTypeHelper<T, common::SharedValue>::get_if(
    const ValueData* data) {
  return C::template cast_if<T>(
      absl::visit(GetPtrVisitor<DefaultVisitor<const C*>, C>(), *data));
}

template <typename T>
const T& BaseValue::BaseTypeHelper<T, common::SharedValue>::get(
    const ValueData& data) {
  const T* value = get_if(&data);
  if (value == nullptr) {
    // Throw bad_variant_access without using `throw` keyword.
    return *absl::get<0>(absl::variant<T*, int>(absl::in_place_index<1>, 1));
  }
  return *value;
}

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google

// Hash specialization for parented owned values.
namespace std {
template <typename T>
struct hash<std::pair<google::api::expr::common::ValueRef, T*>> {
  std::size_t operator()(
      const std::pair<google::api::expr::common::ValueRef, T*>& value) {
    return google::api::expr::internal::Hash(*value.second);
  }
};

template <typename T>
struct hash<std::pair<google::api::expr::common::ValueRef, T>> {
  std::size_t operator()(
      const std::pair<google::api::expr::common::ValueRef, T>& value) {
    return google::api::expr::internal::Hash(value.second);
  }
};

}  // namespace std

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_VALUE_INTERNAL_H_H_
