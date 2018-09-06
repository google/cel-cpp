#include "internal/ref_countable.h"

namespace google {
namespace api {
namespace expr {
namespace internal {

RefCountable::~RefCountable() { assert(unowned()); }

std::size_t RefCountable::owner_count() const {
  return refcount_.load(std::memory_order_acquire);
}

bool RefCountable::single_owner() const {
  // If we are the sole owner, only we have a view of the refcount, so no memory
  // barrier needed.
  return refcount_.load(std::memory_order_relaxed) == 1;
}

bool RefCountable::unowned() const {
  // If refcounting is not being used, then this value must not have changed
  // since construction. If refcounting is being used and this value is 0, then
  // a call to unowned() is inherently racy all ready. So no memory barrier
  // needed.
  return refcount_.load(std::memory_order_relaxed) == 0;
}

void RefCountable::Ref() const {
  refcount_.fetch_add(1, std::memory_order_relaxed);
}

bool RefCountable::Unref() const {
  auto prev_value = refcount_.fetch_sub(1, std::memory_order_release);
  assert(prev_value > 0);
  if (prev_value == 1) {
    std::atomic_thread_fence(std::memory_order_acquire);
    return true;
  }
  return false;
}

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google
