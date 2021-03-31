// Converter functions from common c++ representations to Value.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_CONVERTERS_H_
#define THIRD_PARTY_CEL_CPP_COMMON_CONVERTERS_H_

#include <memory>

#include "absl/meta/type_traits.h"
#include "common/parent_ref.h"
#include "common/value.h"
#include "internal/list_impl.h"
#include "internal/map_impl.h"
#include "internal/types.h"

namespace google {
namespace api {
namespace expr {
namespace common {

// Converters for native c++ list types.

// Creates a Value from the given list.
template <Value::Kind ValueKind, typename T>
Value ValueFromList(T&& value);

// Creates a Value from the given list.
template <Value::Kind ValueKind, typename T>
Value ValueFromList(std::unique_ptr<T> value);

// Creates a Value for the given list with a reference to the given parent.
// If 'parent' is not provided, `value` must live longer than the returned
// Value.
template <Value::Kind ValueKind, typename T>
Value ValueForList(T* value, ParentRef parent = NoParent());

// Converters for native c++ map types.

// Creates a Value from the given map.
template <Value::Kind KeyKind, Value::Kind ValueKind, typename T>
Value ValueFromMap(T&& value);

// Creates a Value from the given map.
template <Value::Kind KeyKind, Value::Kind ValueKind, typename T>
Value ValueFromMap(std::unique_ptr<T> value);

// Creates a Value for the given list with a reference to the given parent.
// If 'parent' is not provided, `value` must live longer than the returned
// Value.
template <Value::Kind KeyKind, Value::Kind ValueKind, typename T>
Value ValueForMap(T* value, ParentRef parent = NoParent());

// Creates a Value from the given list.
template <Value::Kind ValueKind, typename T>
Value ValueFromList(T&& value) {
  static_assert(!std::is_pointer<T>::value, "use ValueForList");
  return Value::MakeList<
      internal::ListWrapper<ValueKind, absl::remove_cvref_t<T>>>(
      std::forward<T>(value));
}

// Creates a Value from the given list.
template <Value::Kind ValueKind, typename T>
Value ValueFromList(std::unique_ptr<T> value) {
  return Value::MakeList<
      internal::ListWrapper<ValueKind, T, internal::OwnedPtr>>(
      std::move(value));
}

// Creates a Value for the given map with a reference to the given parent.
// If 'parent' is not provided, `value` must live longer than the returned
// Value.
template <Value::Kind ValueKind, typename T>
Value ValueForList(T* value, ParentRef parent) {
  if (!parent.has_value()) {
    // Parent does not support refs, so copy the value.
    return ValueFromList<ValueKind>(*value);
  }
  if (parent->RequiresReference()) {
    // Create with reference.
    return Value::MakeList<internal::ListWrapper<
        ValueKind, T, internal::ParentOwnedPtr<ValueRef>>>(parent->GetRef(),
                                                           value);
  }
  // Parent is not provided.
  return Value::MakeList<
      internal::ListWrapper<ValueKind, T, internal::UnownedPtr>>(value);
}

// Converters for native c++ map types.

// Creates a Value from the given map.
template <Value::Kind KeyKind, Value::Kind ValueKind, typename T>
Value ValueFromMap(T&& value) {
  static_assert(!std::is_pointer<T>::value, "use ValueForList");
  return Value::MakeMap<
      internal::MapWrapper<KeyKind, ValueKind, absl::remove_cvref_t<T>>>(
      std::forward<T>(value));
}

// Creates a Value from the given map.
template <Value::Kind KeyKind, Value::Kind ValueKind, typename T>
Value ValueFromMap(std::unique_ptr<T> value) {
  return Value::MakeMap<
      internal::MapWrapper<KeyKind, ValueKind, T, internal::OwnedPtr>>(
      std::move(value));
}

// Creates a Value for the given list with a reference to the given parent.
// If 'parent' is not provided, `value` must live longer than the returned
// Value.
template <Value::Kind KeyKind, Value::Kind ValueKind, typename T>
Value ValueForMap(T* value, ParentRef parent) {
  if (!parent.has_value()) {
    // Parent does not support refs, so copy the value.
    return ValueFromMap<KeyKind, ValueKind>(*value);
  }
  if (parent->RequiresReference()) {
    // Create with reference.
    return Value::MakeMap<internal::MapWrapper<
        KeyKind, ValueKind, T, internal::ParentOwnedPtr<ValueRef>>>(
        parent->GetRef(), value);
  }
  // Parent is not provided.
  return Value::MakeMap<
      internal::MapWrapper<KeyKind, ValueKind, T, internal::UnownedPtr>>(value);
}

}  // namespace common
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_PROTO_CONVERTERS_H_
