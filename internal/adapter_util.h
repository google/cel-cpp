/**
 * Utilities for adapters.
 *
 * Adapters are visitors that accept a single argument.
 *
 * The primary utilities provided in this library include:
 * - MaybeAdapt/MaybeAdaptResult: Tries to apply the given adapter
 * if possible, otherwise returns the given value unchanged.
 * - VisitorAdapter/AdaptVisitor: A visitor that tries to apply the
 * given adapter to every argument before passing those arguments on to the
 * wrapped visitor.
 * - CompositeAdapter: An adapter that (mabye) applies the provided adapters in
 * order.
 */

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_ADAPTER_UTIL_H_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_ADAPTER_UTIL_H_H_

#include <utility>

#include "internal/visitor_util.h"

namespace google {
namespace api {
namespace expr {
namespace internal {

/** An adapter that passes through the give value unchanged. */
struct IdentityAdapter {
  template <typename T>
  T&& operator()(T&& value) {
    return std::forward<T>(value);
  }
};

/** The result of a call to MaybeAdapt. */
template <typename Adapter, typename T>
using MaybeAdaptResultType = MaybeVisitResultType<Adapter, IdentityAdapter, T>;

/**
 * A helper function that applies the adapter if a suitable overload is found,
 * otherwise the value is returned unchanged.
 */
template <typename Adapter, typename T>
MaybeAdaptResultType<Adapter, T> MaybeAdapt(Adapter&& adpt, T&& value) {
  return MaybeVisit(std::forward<Adapter>(adpt), IdentityAdapter(),
                    std::forward<T>(value));
}

/**
 * (Maybe) applies `Adapter` to every argument before passing them to Visitor.
 */
template <typename Visitor, typename Adapter>
class VisitorAdapter {
 public:
  VisitorAdapter() {}
  explicit VisitorAdapter(Visitor&& vis) : vis_(std::forward<Visitor>(vis)) {}
  VisitorAdapter(Visitor&& vis, Adapter&& adapter)
      : vis_(std::forward<Visitor>(vis)),
        adapter_(std::forward<Adapter>(adapter)) {}

  template <typename... Args>
  VisitResultType<Visitor, MaybeAdaptResultType<Adapter, Args>...> operator()(
      Args&&... args) {
    return vis_(MaybeAdapt(adapter_, args)...);
  }

 private:
  Visitor vis_;
  Adapter adapter_;
};

template <typename Visitor, typename Adapter>
VisitorAdapter<Visitor, Adapter> AdaptVisitor(Visitor&& vis,
                                              Adapter&& adapter) {
  return VisitorAdapter<Visitor, Adapter>(std::forward<Visitor>(vis),
                                          std::forward<Adapter>(adapter));
}

/** An adapter that (maybe) applies the given adapters in order. */
template <typename... Adapters>
class CompositeAdapter;

// Only a single adapter.
template <typename Adapter>
class CompositeAdapter<Adapter> {
 public:
  CompositeAdapter() = default;
  CompositeAdapter(const CompositeAdapter&) = default;
  CompositeAdapter(CompositeAdapter&&) = default;
  explicit CompositeAdapter(Adapter&& adpt)
      : adpt_(std::forward<Adapter>(adpt)) {}

  template <typename T>
  using ResultType = MaybeAdaptResultType<Adapter, T>;

  template <typename T>
  ResultType<T> operator()(T&& value) {
    return MaybeAdapt(adpt_, std::forward<T>(value));
  }

 private:
  Adapter adpt_;
};

// Multiple adapters, so pull of the head, and construct a new adapter from
// the tail. Then use MaybeVisit to try and visit Head first.
template <typename Head, typename... Tail>
class CompositeAdapter<Head, Tail...> {
 private:
  using Adapter = Head;
  using NextAdapter = CompositeAdapter<Tail...>;

 public:
  template <typename T>
  using ResultType =
      VisitResultType<NextAdapter, MaybeAdaptResultType<Adapter, T>>;

  CompositeAdapter() = default;
  CompositeAdapter(const CompositeAdapter&) = default;
  CompositeAdapter(CompositeAdapter&&) = default;
  CompositeAdapter(Adapter&& adpt, Tail&&... next)
      : adpt_(std::forward<Adapter>(adpt_)),
        next_(std::forward<Tail>(next)...) {}

  template <typename T>
  ResultType<T> operator()(T&& value) {
    return next_(MaybeAdapt(adpt_, std::forward<T>(value)));
  }

 private:
  Adapter adpt_;
  NextAdapter next_;
};

/** Helper function to construct a CompositeAdapter. */
template <typename... Adapters>
CompositeAdapter<Adapters...> MakeCompositeAdapter(Adapters&&... vis) {
  return CompositeAdapter<Adapters...>(std::forward<Adapters>(vis)...);
}

/** An adapter wrapper that restricts adaptation to the specified types. */
template <typename Adapter, typename... Types>
struct StrictAdapter {
  template <typename... Args>
  StrictAdapter(Args&&... args) : adpt(std::forward<Args>(args)...) {}

  template <typename T>
  specialize_ift<arg_in<T, Types...>, VisitResultType<Adapter, T&&>> operator()(
      T&& value) {
    return adpt(std::forward<T>(value));
  }

  Adapter adpt;
};

/** An adapter wrapper that applies Adapter and converts the result to T. */
template <typename Adapter, typename T>
struct ConvertAdapter {
  template <typename... Args>
  ConvertAdapter(Args&&... args) : adpt(std::forward<Args>(args)...) {}

  template <typename U>
  // Only enable for types accepted by Adapter
  specialize_ifd<T, VisitResultType<Adapter, U&&>> operator()(U&& value) {
    return T(adpt(std::forward<U>(value)));
  }

  Adapter adpt;
};

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_ADAPTER_UTIL_H_H_
