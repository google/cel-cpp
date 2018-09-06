#include "internal/types.h"

#include "gtest/gtest.h"

namespace google {
namespace api {
namespace expr {
namespace internal {

TEST(Types, Numeric) {
  static_assert(!is_numeric<bool>::value, "bool is not numeric");
}

TEST(Types, String) {
  static_assert(!is_string<std::nullptr_t>::value, "nullptr is not a string");
}

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google
