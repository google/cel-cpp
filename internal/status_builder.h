#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_STATUS_BUILDER_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_STATUS_BUILDER_H_

#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/status/status.h"

namespace cel::internal {

class StatusBuilder;

template <typename Invocable, typename Argument, typename Expected>
inline constexpr bool kResultMatches =
    std::is_same_v<std::decay_t<std::invoke_result_t<Invocable, Argument>>,
                   Expected>;

template <typename Adaptor, typename Builder>
using EnableIfStatusBuilder =
    std::enable_if_t<kResultMatches<Adaptor, Builder, StatusBuilder>,
                     std::invoke_result_t<Adaptor, Builder>>;

template <typename Adaptor, typename Builder>
using EnableIfStatus =
    std::enable_if_t<kResultMatches<Adaptor, Builder, absl::Status>,
                     std::invoke_result_t<Adaptor, Builder>>;

class StatusBuilder final {
 public:
  StatusBuilder() = default;

  explicit StatusBuilder(const absl::Status& status) : status_(status) {}

  StatusBuilder(const StatusBuilder&) = default;

  StatusBuilder(StatusBuilder&&) = default;

  ~StatusBuilder() = default;

  StatusBuilder& operator=(const StatusBuilder&) = default;

  StatusBuilder& operator=(StatusBuilder&&) = default;

  bool ok() const { return status_.ok(); }

  absl::StatusCode code() const { return status_.code(); }

  operator absl::Status() const& { return status_; }  // NOLINT

  operator absl::Status() && { return std::move(status_); }  // NOLINT

  template <typename Adaptor>
  auto With(
      Adaptor&& adaptor) & -> EnableIfStatusBuilder<Adaptor, StatusBuilder&> {
    return std::forward<Adaptor>(adaptor)(*this);
  }

  template <typename Adaptor>
  ABSL_MUST_USE_RESULT auto With(
      Adaptor&& adaptor) && -> EnableIfStatusBuilder<Adaptor, StatusBuilder&&> {
    return std::forward<Adaptor>(adaptor)(std::move(*this));
  }

  template <typename Adaptor>
  auto With(Adaptor&& adaptor) & -> EnableIfStatus<Adaptor, StatusBuilder&> {
    return std::forward<Adaptor>(adaptor)(*this);
  }

  template <typename Adaptor>
  ABSL_MUST_USE_RESULT auto With(
      Adaptor&& adaptor) && -> EnableIfStatus<Adaptor, StatusBuilder&&> {
    return std::forward<Adaptor>(adaptor)(std::move(*this));
  }

 private:
  absl::Status status_;
};

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_STATUS_BUILDER_H_
