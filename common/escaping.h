#ifndef THIRD_PARTY_CEL_CPP_PARSER_UNESCAPE_H_
#define THIRD_PARTY_CEL_CPP_PARSER_UNESCAPE_H_

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"

namespace google {
namespace api {
namespace expr {
namespace parser {

// Unescape takes a quoted string, unquotes, and unescapes it.
absl::optional<std::string> unescape(const std::string& s, bool is_bytes);

// Takes a string, and escapes values according to CEL and quotes
std::string escapeAndQuote(absl::string_view str);

}  // namespace parser
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_PARSER_UNESCAPE_H_
