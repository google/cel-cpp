#ifndef THIRD_PARTY_CEL_CPP_COMMON_ID_H_
#define THIRD_PARTY_CEL_CPP_COMMON_ID_H_

#include "internal/cel_printer.h"
#include "internal/handle.h"
#include "internal/hash_util.h"

namespace google {
namespace api {
namespace expr {
namespace common {

// A expression, statement, or variable id.
class Id : public internal::Handle<int32_t, Id> {
 public:
  constexpr explicit Id(int32_t value) : Handle(value) {}

  inline std::string ToDebugString() const {
    return internal::ToCallString("Id", value_);
  }
};

inline std::ostream& operator<<(std::ostream& os, Id arg) {
  return os << arg.ToDebugString();
}

}  // namespace common
}  // namespace expr
}  // namespace api
}  // namespace google

namespace std {

template <>
struct hash<google::api::expr::common::Id>
    : google::api::expr::common::Id::Hasher {};

}  // namespace std

#endif  // THIRD_PARTY_CEL_CPP_COMMON_ID_H_
