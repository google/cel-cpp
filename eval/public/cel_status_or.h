#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_STATUS_OR_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_STATUS_OR_H_

#include "eval/public/cel_status.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {
namespace util {

// CelStatusOr<T> is either a non-OK status, or an OK status and an object of
// type T. It is used as the return type of functions that can return either a
// bad status or a value.
template <typename T>
class StatusOr {
 public:
  explicit StatusOr() : status_(OkStatus()) {}

  // Expect !util::IsOk(status).  If not error code set to INTERNAL
  // Not marking explicit, to allow conversion.
  StatusOr(const Status& status)  // NOLINT
      : status_(IsOk(status) ? MakeStatus(google::rpc::Code::INTERNAL,
                                          "bad StatusOr constructor call")
                             : status) {}

  // Not marking explicit, to allow conversion.
  StatusOr(const T& value)  // NOLINT
      : status_(OkStatus()), value_(value) {}

  // Not marking explicit, to allow conversion.
  StatusOr(T&& value)  // NOLINT
      : status_(OkStatus()), value_(std::move(value)) {}

  // Not marking explicit, to allow conversion.
  template <typename U>
  explicit StatusOr(const StatusOr<U>& other)  // NOLINT
      : status_(other.status_) {
    if (util::IsOk(other)) {
      value_ = other.value_;
    }
  }

  template <typename U>
  explicit StatusOr(StatusOr<U>&& other)  // NOLINT
      : status_(std::move(other.status_)) {
    if (ok()) {
      value_ = std::move(other.value_);
    }
  }

  template <typename U>
  StatusOr& operator=(const StatusOr<U>& other) {
    status_ = other.status_;
    if (util::IsOk(other)) {
      value_ = other.value_;
    }
    return *this;
  }

  template <typename U>
  StatusOr& operator=(StatusOr<U>&& other) {
    status_ = std::move(other.status_);
    if (util::IsOk(other)) {
      value_ = std::move(other.value_);
    }
    return *this;
  }

  // Expect !util::IsOk(status).  If not error code set to INTERNAL
  StatusOr& operator=(const Status& status) {
    status_ = IsOk(status) ? MakeStatus(google::rpc::Code::INTERNAL,
                                        "bad CelStatusOr constructor call")
                           : status;
    return *this;
  }

  StatusOr& operator=(Status&& status) {
    if (IsOk(status)) {
      status_ = MakeStatus(google::rpc::Code::INTERNAL,
                           "bad CelStatusOr constructor call");
    } else {
      status_ = std::move(status);
    }
    return *this;
  }

  bool ok() const { return IsOk(status_); }
  const Status& status() const { return status_; }

  const T& ValueOrDie() const {
    if (!ok()) {
      abort();
    }
    return value_;
  }

  T& ValueOrDie() {
    if (!ok()) {
      abort();
    }
    return value_;
  }

 private:
  Status status_;
  T value_;  // set iff status is OK
};

template <typename T>
bool IsOk(const StatusOr<T>& status) {
  return IsOk(status.status());
}

}  // namespace util
}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_STATUS_OR_H_
