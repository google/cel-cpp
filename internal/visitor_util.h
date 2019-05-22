/**
 * Utilities for working with visitor patterns.
 *
 * Terminology:
 * - A 'visitor' is a callable that can be passed arguments and returns
 * a result.
 * - A 'mixin visitor' is a visitor only handles a subset of cases. These can
 * be merged using `OrderedVisitor`.
 *
 * The primary utilities provided in this library include:
 * - VisitResultType: The return type of apply a visitor to a list of arguments.
 * - MaybeVisit/MaybeVisitResult: Uses the given visitor if
 * possible, otherwise uses a given fallback visitor.
 * - OrderedVisitor: A visitor that calls the first provided visitor that
 * supports the given arguments. Useful for mixin visitor or to resolve
 * call ambiguities.
 */

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_VISITOR_UTIL_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_VISITOR_UTIL_H_

#include <cmath>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "internal/specialize.h"
#include "internal/types.h"

namespace google {
namespace api {
namespace expr {
namespace internal {

/**
 * The result type of applying the visitor once.
 *
 * Undefined when a matching overload is not present or *ambiguous*.
 */
template <typename Visitor, typename... Args>
using VisitResultType = typename std::result_of<Visitor(Args&&...)>::type;

// Specialization for when Visitor can be applied to the arguments.
template <typename Visitor, typename Fallback, typename... Args>
VisitResultType<Visitor, Args...> MaybeVisitImpl(Visitor&& vis,
                                                 Fallback&& fallback,
                                                 specialize, Args&&... args) {
  return vis(std::forward<Args>(args)...);
}

// Specialization for when a specialized Visitor can be applied to the
// arguments.
template <typename Visitor, typename Fallback, typename... Args>
VisitResultType<Visitor, Args..., specialize> MaybeVisitImpl(
    Visitor&& vis, Fallback&& fallback, specialize, Args&&... args) {
  return vis(std::forward<Args>(args)..., specialize());
}

// Specialization for when Visitor cannot be applied to the arguments.
template <typename Visitor, typename Fallback, typename... Args>
VisitResultType<Fallback, Args...> MaybeVisitImpl(Visitor&& vis,
                                                  Fallback&& fallback, general,
                                                  Args&&... args) {
  return fallback(std::forward<Args>(args)...);
}

/** The type returned by a call to MaybeVisit. */
template <typename Visitor, typename Fallback, typename... Args>
using MaybeVisitResultType =
    decltype(MaybeVisitImpl(inst_of<Visitor&&>(), inst_of<Fallback&&>(),
                            specialize(), inst_of<Args&&>()...));

/**
 * A helper function that tries to apply Visitor, and falls back on Fallback
 * when it cannot.
 */
template <typename Visitor, typename Fallback, typename... Args>
MaybeVisitResultType<Visitor, Fallback, Args...> MaybeVisit(Visitor&& vis,
                                                            Fallback&& fallback,
                                                            Args&&... args) {
  return MaybeVisitImpl(std::forward<Visitor>(vis),
                        std::forward<Fallback>(fallback), specialize(),
                        std::forward<Args>(args)...);
}

/** A visitor that delegates to the first applicable visitor. */
template <typename... Visitors>
class OrderedVisitor;

// Only a single visitor, so expose it direclty.
template <typename Visitor>
class OrderedVisitor<Visitor> : public Visitor {
 public:
  OrderedVisitor() = default;
  OrderedVisitor(const OrderedVisitor&) = default;
  OrderedVisitor(OrderedVisitor&&) = default;
  explicit OrderedVisitor(Visitor&& vis)
      : Visitor(std::forward<Visitor>(vis)) {}

  template <typename... Args>
  using ResultType = VisitResultType<Visitor, Args...>;
};

// Multiple visitors, so pull of the head, and construct a new visitor from
// the tail. Then use MaybeVisit to try and visit Head first.
template <typename Head, typename... Tail>
class OrderedVisitor<Head, Tail...> {
 private:
  using Visitor = Head;
  using Fallback = OrderedVisitor<Tail...>;

 public:
  template <typename... Args>
  using ResultType = MaybeVisitResultType<Visitor, Fallback, Args...>;

  OrderedVisitor() = default;
  OrderedVisitor(const OrderedVisitor&) = default;
  OrderedVisitor(OrderedVisitor&&) = default;
  OrderedVisitor(Visitor&& vis, Tail&&... fallback)
      : vis_(std::forward<Visitor>(vis)),
        fallback_(std::forward<Tail>(fallback)...) {}

  template <typename... Args>
  ResultType<Args...> operator()(Args&&... args) {
    return MaybeVisit(vis_, fallback_, std::forward<Args>(args)...);
  }

 private:
  Visitor vis_;
  Fallback fallback_;
};

/** Helper function to construct an OrderedVisitor. */
template <typename... Visitors>
OrderedVisitor<Visitors...> MakeOrderedVisitor(Visitors&&... vis) {
  return OrderedVisitor<Visitors...>(std::forward<Visitors>(vis)...);
}

/** A visitor that ignores all arguments and returns the given value. */
template <typename T>
struct DefaultVisitor {
  template <typename... Args>
  T operator()(Args&&... args) {
    return T();
  }
};

// A visitor that throws a absl::bad_variant_access exception.
template <typename R>
struct BadAccessVisitor {
  template <typename... Args>
  R operator()(Args&&...) {
    // Throw bad_variant_access without using `throw` keyword.
    return absl::get<0>(absl::variant<R, int>(absl::in_place_index<1>, 1));
  }
};

/**
 * A visitor that check the equality of two arguments if they are the same type.
 *
 * If the arguments are of different types, false is returned, otherwise the
 * result of the == operator is returned.
 */
struct StrictEqVisitor {
  // Specialization for double that treats NaN as equal.
  bool operator()(double lhs, double rhs) {
    if (lhs == rhs) {
      return true;
    }
    return std::isnan(lhs) && std::isnan(rhs);
  }

  template <typename T>
  bool operator()(const T& lhs, const T& rhs) {
    return lhs == rhs;
  }

  template <typename T, typename S>
  bool operator()(const T& lhs, const S& rhs) {
    return false;
  }
};

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_VISITOR_UTIL_H_
