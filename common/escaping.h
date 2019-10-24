#ifndef THIRD_PARTY_CEL_CPP_PARSER_UNESCAPE_H_
#define THIRD_PARTY_CEL_CPP_PARSER_UNESCAPE_H_

#include <optional>
#include <string>

namespace google {
namespace api {
namespace expr {
namespace parser {

// Unescape takes a quoted string, unquotes, and unescapes it.
std::optional<std::string> unescape(const std::string& s, bool is_bytes);

// Takes a string, and escapes values according to CEL and quotes
std::string escapeAndQuote(std::string_view str);

}  // namespace parser
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_PARSER_UNESCAPE_H_
