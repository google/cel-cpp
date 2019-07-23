// Helper classes for creating 'view' values using parent references.
//
// Can class that inherits from 'SharedValue' can be used as the parent of
// of another value. Note that `SharedValue` should typically not be inherited
// from directly, instead inherit from `List`, `Map`, or `Object`.
//
// Shared values support:
// - Down-casting to a specific implementation through `Shared::cast_if`.
// - Creating self-references that can be used to create 'views' of the data
// owned by the shared value. For example, a 'list of strings' can
// create element `Value` instances that reference (vs copy) the underlying
// string.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_PARENT_REF_H_
#define THIRD_PARTY_CEL_CPP_COMMON_PARENT_REF_H_

#include "absl/types/optional.h"
#include "internal/ref_countable.h"

namespace google {
namespace api {
namespace expr {
namespace common {

class SharedValue;

/**
 * An opaque reference to a value that prevents the value from being deleted.
 *
 * Used to support 'views' by allowing the view prevent a parent value from
 * being deleted.
 *
 * Only constructable via `RefProvider::GetRef`.
 */
class ValueRef {
 public:
  // Value semantics.
  ValueRef() = default;
  ValueRef(const ValueRef& other) = default;
  ValueRef(ValueRef&& other) = default;
  ValueRef& operator=(const ValueRef& other) = default;
  ValueRef& operator=(ValueRef&& other) = default;

  operator bool() const { return ptr_ != nullptr; }

 private:
  friend class RefProvider;

  internal::ReffedPtr<const SharedValue> ptr_;

  explicit ValueRef(const SharedValue* ptr) : ptr_(ptr) {}
};

/**
 * A class that can create references for a shared value.
 *
 * Only constructable via `SharedValue::SelfRefProvider()`
 */
class RefProvider {
 public:
  RefProvider() = default;

  RefProvider(const RefProvider&) = default;
  RefProvider(RefProvider&&) = default;
  RefProvider& operator=(const RefProvider&) = default;
  RefProvider& operator=(RefProvider&&) = default;

  /**
   * Returns true if a reference is required for the given parent.
   *
   * False when the parent does not own its value, thus ownership need not be
   * tracked.
   */
  bool RequiresReference() const { return ptr_ != nullptr; }

  ValueRef GetRef() const { return ValueRef(ptr_); }

 private:
  friend class SharedValue;
  friend constexpr RefProvider NoParent();

  constexpr explicit RefProvider(const SharedValue* ptr) : ptr_(ptr) {}

  const SharedValue* ptr_;
};

constexpr inline RefProvider NoParent() { return RefProvider(nullptr); }

// The type by which a parent reference provider should be passed around as.
//
// A value of absl::nullopt indicates that the parent cannot be referenced.
using ParentRef = absl::optional<RefProvider>;

/** The base class for custom value implementations. */
class SharedValue : public internal::RefCountable {
 public:
  virtual ~SharedValue() {}

  virtual bool owns_value() const = 0;

  /**
   * Returns a canonical cel expression for the value.
   *
   * Computation may be expensive.
   */
  virtual std::string ToString() const = 0;

  /**
   * Attempts to cast the given Container to the given type.
   *
   * Returns nullptr if value is null or is not the requested type.
   */
  template <typename T>
  static const T* cast_if(const SharedValue* value) {
    return dynamic_cast<const T*>(value);
  }

 protected:
  SharedValue() = default;

  /**
   * Construct a self reference provider, to be passed to a 'view' Value.
   *
   * `this` must live longer than the returned value.
   *
   * Returns absl::nullopt if the container cannot be reffed, in which case
   * all needed data must be copied.
   *
   * No synchronization is performed until ParentRef->GetRef() is called, so
   * calls to this function do not incur a synchronization performance penalty.
   */
  ParentRef SelfRefProvider() const;
};

}  // namespace common
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_COMMON_PARENT_REF_H_
