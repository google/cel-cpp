#ifndef THIRD_PARTY_CEL_CPP_COMMON_UNKNOWN_H_
#define THIRD_PARTY_CEL_CPP_COMMON_UNKNOWN_H_

#include "absl/types/span.h"
#include "common/id.h"

namespace google {
namespace api {
namespace expr {
namespace common {

/** An unknown CEL value. */
class Unknown {
 public:
  explicit Unknown(Id id);
  explicit Unknown(absl::Span<const Id> ids);

  const std::set<Id>& ids() const { return ids_; }

  inline bool operator==(const Unknown& rhs) const {
    return this == &rhs || ids_ == rhs.ids_;
  }

  inline bool operator!=(const Unknown& rhs) const { return !(*this == rhs); }

  /** The hash code for this value. */
  std::size_t hash_code() const;

  /**
   * A std::string useful for debugging.
   *
   * Format may change, and computation may be expensive.
   */
  std::string ToString() const;

 private:
  std::set<Id> ids_;
};

inline std::ostream& operator<<(std::ostream& os, const Unknown& value) {
  return os << value.ToString();
}

}  // namespace common
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_COMMON_UNKNOWN_H_
