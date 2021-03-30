#ifndef THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_REFERENCE_COUNTED_CEL_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_REFERENCE_COUNTED_CEL_VALUE_H_

#include <atomic>
#include <cstddef>
#include <memory>
#include <utility>

#include "absl/meta/type_traits.h"
#include "internal/holder.h"
#include "internal/specialize.h"

namespace google {
namespace api {
namespace expr {
namespace internal {

/**
 * A base class for optionally reference counted objects.
 *
 * Like std::shared_ptr, reference counting is optional and only active when
 * used with a smart pointer (ReffedPtr). The main difference is that the
 * counter is embedded within the object, so the size of the smart pointer
 * is equal to that of a normal pointer.
 *
 * Unlike typical reference counting implementations, inheriting from this class
 * does not restrict the function of the subclass. For example, subclasses can
 * be constructed on the stack, passed by value, or used with other smart
 * pointer classes (e.g. std::unique_ptr). Additionally, a virtual destructor
 * is not required (although it is often useful).
 */
class RefCountable {
 protected:
  // Cannot be constructed or destructed directly.
  constexpr RefCountable() : refcount_(0) {}
  ~RefCountable();

  // The refcount is not copied.
  constexpr RefCountable(const RefCountable&) : refcount_(0) {}
  RefCountable& operator=(const RefCountable&) { return *this; }

  std::size_t owner_count() const;

  /** Returns true if this object only has a single owner. */
  bool single_owner() const;

  /** Returns true if this object is not owned by a ReffedPtr. */
  bool unowned() const;

 private:
  template <typename T>
  friend class ReffedPtr;

  mutable std::atomic<std::size_t> refcount_;

  /** Increments the ref count. */
  void Ref() const;

  /** Decrements the ref count and returns true if the count becomes 0. */
  bool Unref() const;
};

/**
 * A smart pointer to a reference countable object.
 *
 * @tparam T The type to point to. Should either be 'final' or have a virtual
 * destructor.
 */
template <typename T>
class ReffedPtr {
 public:
  template <typename... Args>
  static ReffedPtr Make(Args&&... args) {
    return ReffedPtr(new T(std::forward<Args>(args)...));
  }

  constexpr ReffedPtr() {}
  constexpr ReffedPtr(std::nullptr_t) {}
  explicit ReffedPtr(T* ptr) : ptr_(ptr) { MaybeRef(); }
  explicit ReffedPtr(std::unique_ptr<T> ptr) : ptr_(ptr.release()) {
    MaybeRef();
  }

  ReffedPtr(const ReffedPtr& other) : ptr_(other.ptr_) { MaybeRef(); }
  template <typename U, typename = specialize_ift<std::is_convertible<U*, T*>>>
  ReffedPtr(const ReffedPtr<U>& other) : ptr_(other.ptr_) {
    MaybeRef();
  }
  ReffedPtr(ReffedPtr&& other) : ptr_(other.ptr_) { other.ptr_ = nullptr; }
  template <typename U, typename = specialize_ift<std::is_convertible<U*, T*>>>
  ReffedPtr(ReffedPtr<U>&& other) : ptr_(other.ptr_) {
    other.ptr_ = nullptr;
  }

  ReffedPtr& operator=(const ReffedPtr& other) { return EqImpl(other); }
  template <typename U, typename = specialize_ift<std::is_convertible<U*, T*>>>
  ReffedPtr& operator=(const ReffedPtr<U>& other) {
    return EqImpl(other);
  }

  ReffedPtr& operator=(ReffedPtr&& other) { return EqImpl(std::move(other)); }
  template <typename U, typename = specialize_ift<std::is_convertible<U*, T*>>>
  ReffedPtr& operator=(ReffedPtr<U>&& other) {
    return EqImpl(std::move(other));
  }

  inline ReffedPtr& operator=(std::nullptr_t) {
    reset();
    return *this;
  }
  constexpr inline bool operator==(std::nullptr_t) const {
    return ptr_ == nullptr;
  }
  constexpr inline bool operator!=(std::nullptr_t) const {
    return ptr_ != nullptr;
  }

  void reset();

  ~ReffedPtr() { reset(); }

  constexpr T* get() const { return ptr_; }
  constexpr T& operator*() const { return *ptr_; }
  constexpr T* operator->() const { return get(); }

 private:
  template <typename U>
  friend class ReffedPtr;

  T* ptr_ = nullptr;

  void MaybeRef() {
    if (ptr_ != nullptr) {
      ptr_->Ref();
    }
  }

  template <typename U>
  ReffedPtr& EqImpl(const ReffedPtr<U>& other) {
    reset();
    ptr_ = other.ptr_;
    MaybeRef();
    return *this;
  }

  template <typename U>
  ReffedPtr& EqImpl(ReffedPtr<U>&& other) {
    reset();
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
    return *this;
  }
};

template <typename T, typename... Args>
ReffedPtr<T> MakeReffed(Args&&... args) {
  return ReffedPtr<T>::Make(std::forward<Args>(args)...);
}

template <typename T>
ReffedPtr<T> MakeReffed(std::unique_ptr<T> value) {
  return ReffedPtr<T>(std::move(value));
}

template <typename T>
ReffedPtr<T> WrapReffed(T* value) {
  return ReffedPtr<T>(value);
}

/**
 * A ReffedPtr based HolderPolicy.
 */
struct RefPtr : BaseHolderPolicy {
  constexpr static const bool kOwnsValue = true;

  template <typename T>
  using ValueType = ReffedPtr<T>;

  template <typename T>
  static T& get(ReffedPtr<T>& value) {
    return *value;
  }

  template <typename T>
  static const T& get(const ReffedPtr<T>& value) {
    return *value;
  }
};

template <typename T>
using RefPtrHolder = Holder<T, RefPtr>;

/**
 * A reference countable holder.
 *
 * @see Holder
 */
template <typename T, typename HolderPolicy>
class RefCountableHolder : public RefCountable {
 public:
  template <typename... Args>
  explicit RefCountableHolder(Args&&... args)
      : value_(std::forward<Args>(args)...) {}

  T& value() { return HolderPolicy::template get<T>(value_); }
  const T& value() const { return HolderPolicy::template get<T>(value_); }

 private:
  typename HolderPolicy::template ValueType<T> value_;
};

/** A reference counting HolderPolicy */
template <typename HolderPolicy>
struct Ref : BaseHolderPolicy {
  constexpr static const bool kOwnsValue = true;

  template <typename T>
  using ValueType = ReffedPtr<RefCountableHolder<T, HolderPolicy>>;

  // Forward to Make function.
  template <typename V, typename T, typename... Args>
  static V Create(Args&&... args) {
    return V::Make(std::forward<Args>(args)...);
  }

  template <typename T>
  static T& get(ReffedPtr<RefCountableHolder<T, HolderPolicy>>& value) {
    return value->value();
  }

  template <typename T>
  static const T& get(
      const ReffedPtr<RefCountableHolder<T, HolderPolicy>>& value) {
    return value->value();
  }
};

template <typename T>
using RefCopyHolder = Holder<T, Ref<Copy>>;

// If the size of T is smaller than MAX, it is stored inline, otherwise
// it is stored in a heap allocated, reference counted holder.
template <typename T, std::size_t MAX = sizeof(void*)>
using SizeLimitHolder =
    absl::conditional_t<sizeof(T) <= MAX, Holder<const T, Copy>,
                        Holder<const T, Ref<Copy>>>;

template <typename T>
void ReffedPtr<T>::reset() {
  if (ptr_ == nullptr) {
    return;
  }
  if (ptr_->Unref()) {
    delete ptr_;
  }
  ptr_ = nullptr;
}

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_INTERNAL_REFERENCE_COUNTED_CEL_VALUE_H_
