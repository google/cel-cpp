#ifndef THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_HOLDERS_H_
#define THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_HOLDERS_H_

#include <algorithm>
#include <memory>

#include "internal/port.h"
#include "internal/specialize.h"
#include "internal/types.h"
#include "internal/visitor_util.h"

namespace google {
namespace api {
namespace expr {
namespace internal {

/**
 * A class that holds a value using the given holder policy.
 *
 * This class is largely used to smooth over the awkward syntax for
 * referencing dependent template type names (see the type of value_).
 *
 * @tparam T the type of value to hold.
 * @tparam HolderPolicy the policy by which the value should be held.
 */
template <typename T, typename HolderPolicy>
class Holder {
  using ValueType = typename HolderPolicy::template ValueType<T>;

 public:
  Holder(Holder&) = default;
  Holder(const Holder&) = default;
  Holder(Holder&&) = default;
  Holder& operator=(const Holder&) = default;
  Holder& operator=(Holder&&) = default;

  template <typename... Args>
  explicit Holder(Args&&... args)
      : value_(HolderPolicy::template Create<ValueType, T>(
            std::forward<Args>(args)...)) {}

  T& value() { return HolderPolicy::template get<T>(value_); }
  const T& value() const { return HolderPolicy::template get<T>(value_); }

  T& operator*() { return value(); }
  const T& operator*() const { return value(); }

  T* operator->() { return &value(); }
  const T* operator->() const { return &value(); }

 private:
  ValueType value_;
};

struct BaseHolderPolicy {
  /**
   * In-place create by default.
   * @tparam V the hold's value type.
   * @tparam T the type being held.
   */
  template <typename V, typename T, typename... Args>
  static V Create(Args&&... args) {
    return V(std::forward<Args>(args)...);
  }
};

/** A holder policy that keeps a copy of the value. */
struct Copy : BaseHolderPolicy {
  constexpr static const bool kOwnsValue = true;

  template <typename T>
  using ValueType = remove_const_t<T>;

  template <typename T>
  static T& get(T& value) {
    return value;
  }

  template <typename T>
  static const T& get(const T& value) {
    return value;
  }
};

template <typename T>
using CopyHolder = Holder<T, Copy>;

/** A holder policy that keeps a unique_ptr of the given type. */
struct OwnedPtr : BaseHolderPolicy {
  constexpr static const bool kOwnsValue = true;

  template <typename T>
  using ValueType = std::unique_ptr<T>;

  template <typename T>
  static T& get(std::unique_ptr<T>& value) {
    assert(value != nullptr);
    return *value;
  }

  template <typename T>
  static const T& get(const std::unique_ptr<T>& value) {
    assert(value != nullptr);
    return *value;
  }
};

template <typename T>
using OwnedPtrHolder = Holder<T, OwnedPtr>;

/** A holder policy that keeps a raw pointer of the given type. */
struct UnownedPtr : BaseHolderPolicy {
  constexpr static const bool kOwnsValue = false;

  template <typename T>
  using ValueType = T*;

  template <typename T>
  static T& get(T* value) {
    assert(value != nullptr);
    return *value;
  }

  template <typename T>
  static const T& get(const T* value) {
    assert(value != nullptr);
    return *value;
  }
};

template <typename T>
using UnownedPtrHolder = Holder<T, UnownedPtr>;

/** A holder policy the keeps a reference on a parent container. */
template <typename P, typename HolderPolicy>
struct ParentOwned : BaseHolderPolicy {
  // It owns the value since it owns a ref on the parent.
  constexpr static const bool kOwnsValue = true;

  template <typename T>
  using InternalValueType = typename HolderPolicy::template ValueType<T>;

  template <typename V, typename T, typename... Args>
  static V Create(const P& parent, Args&&... args) {
    return V(parent, HolderPolicy::template Create<InternalValueType<T>, T>(
                         std::forward<Args>(args)...));
  }

  /**
   * The holder stores a pair of a reference to the parent and the real value.
   */
  template <typename T>
  using ValueType = std::pair<P, InternalValueType<T>>;

  /** Returns the real value from the pair. */
  template <typename T>
  static T& get(std::pair<P, InternalValueType<T>>& value) {
    return HolderPolicy::get(value.second);
  }

  /** Returns the real const value from the pair. */
  template <typename T>
  static const T& get(const std::pair<P, InternalValueType<T>>& value) {
    return HolderPolicy::get(value.second);
  }
};

template <typename P>
using ParentOwnedPtr = ParentOwned<P, UnownedPtr>;

template <typename P>
using ParentOwnedCopy = ParentOwned<P, Copy>;

/** An adapter that returns a held value regardless of the holder policy. */
struct HolderAdapter {
  template <typename T, typename P>
  const T& operator()(const Holder<T, P>& value) {
    return value.value();
  }

  template <typename T, typename P>
  T& operator()(Holder<T, P>& value) {
    return value.value();
  }

  template <typename T, typename P>
  T operator()(Holder<T, P>&& value) {
    return std::move(value.value());
  }
};

/**
 * An adapter that returns a pointer to the held value regardless of the holder
 * policy.
 */
struct HolderPtrAdapter {
  template <typename T, typename P>
  const T* operator()(const Holder<T, P>& value) {
    return &value.value();
  }

  template <typename T, typename P>
  T* operator()(Holder<T, P>& value) {
    return &value.value();
  }
};

/** A wrapper that only passes through holders of a specific set of types. */
template <typename Adapter, typename... Types>
struct StrictHolderAdapter : Adapter {
  template <typename... Args>
  StrictHolderAdapter(Args&&... args) : Adapter(std::forward<Args>(args)...) {}

  template <typename T, typename P>
  specialize_ift<arg_in<T, Types...>,
                 VisitResultType<Adapter, const Holder<T, P>&>>
  operator()(const Holder<T, P>& value) {
    return Adapter::operator()(value);
  }

  template <typename T, typename P>
  specialize_ift<arg_in<T, Types...>, VisitResultType<Adapter, Holder<T, P>&>>
  operator()(Holder<T, P>& value) {
    return Adapter::operator()(value);
  }

  template <typename T, typename P>
  specialize_ift<arg_in<T, Types...>, VisitResultType<Adapter, Holder<T, P>&&>>
  operator()(Holder<T, P>&& value) {
    return Adapter::operator()(std::move(value));
  }
};

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_HOLDERS_H_
