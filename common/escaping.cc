#include "common/escaping.h"

#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "util/utf8/public/unicodetext.h"

namespace google {
namespace api {
namespace expr {
namespace parser {

inline std::pair<char, bool> unhex(char c) {
  if ('0' <= c && c <= '9') {
    return std::make_pair(c - '0', true);
  }
  if ('a' <= c && c <= 'f') {
    return std::make_pair(c - 'a' + 10, true);
  }
  if ('A' <= c && c <= 'F') {
    return std::make_pair(c - 'A' + 10, true);
  }
  return std::make_pair(0, false);
}

// unescape_char takes a string input and returns the following info:
//
//   value - the escaped unicode rune at the front of the string.
//   encode - the value should be unicode-encoded
//   tail - the remainder of the input string.
//   err - error value, if the character could not be unescaped.
//
// When encode is true the return value may still fit within a single byte,
// but unicode encoding is attempted which is more expensive than when the
// value is known to self-represent as a single byte.
//
// If is_bytes is set, unescape as a bytes literal so octal and hex escapes
// represent byte values, not unicode code points.
inline std::tuple<std::string, std::string_view, std::string> unescape_char(
    std::string_view s, bool is_bytes) {
  char c = s[0];

  // 1. Character is not an escape sequence.
  if (c >= 0x80 && !is_bytes) {
    UnicodeText ut;
    ut.PointToUTF8(s.data(), s.size());
    auto r = ut.begin();
    char tmp[5];
    int len = r.get_utf8(tmp);
    tmp[len] = '\0';
    return std::make_tuple(std::string(tmp), s.substr(len), "");
  } else if (c != '\\') {
    char tmp[2] = {c, '\0'};
    return std::make_tuple(std::string(tmp), s.substr(1), "");
  }

  // 2. Last character is the start of an escape sequence.
  if (s.size() <= 1) {
    return std::make_tuple("", s,
                           "unable to unescape string, "
                           "found '\\' as last character");
  }

  c = s[1];
  s = s.substr(2);

  char32_t value;
  bool encode = false;

  // 3. Common escape sequences shared with Google SQL
  switch (c) {
    case 'a':
      value = '\a';
      break;
    case 'b':
      value = '\b';
      break;
    case 'f':
      value = '\f';
      break;
    case 'n':
      value = '\n';
      break;
    case 'r':
      value = '\r';
      break;
    case 't':
      value = '\t';
      break;
    case 'v':
      value = '\v';
      break;
    case '\\':
      value = '\\';
      break;
    case '\'':
      value = '\'';
      break;
    case '"':
      value = '"';
      break;
    case '`':
      value = '`';
      break;
    case '?':
      value = '?';
      break;

    // 4. Unicode escape sequences, reproduced from `strconv/quote.go`
    case 'x':
      [[fallthrough]];
    case 'X':
      [[fallthrough]];
    case 'u':
      [[fallthrough]];
    case 'U': {
      int n = 0;
      encode = true;
      switch (c) {
        case 'x':
          [[fallthrough]];
        case 'X':
          n = 2;
          encode = !is_bytes;
          break;
        case 'u':
          n = 4;
          if (is_bytes) {
            return std::make_tuple("", s,
                                   "unable to unescape string "
                                   "(\\u in bytes)");
          }
          break;
        case 'U':
          n = 8;
          if (is_bytes) {
            return std::make_tuple("", s,
                                   "unable to unescape string "
                                   "(\\U in bytes)");
          }
          break;
      }
      char32_t v = 0;
      if (s.size() < n) {
        return std::make_tuple("", s,
                               "unable to unescape string "
                               "(string too short after \\xXuU)");
      }
      for (int j = 0; j < n; ++j) {
        auto x = unhex(s[j]);
        if (!x.second) {
          return std::make_tuple("", s,
                                 "unable to unescape string "
                                 "(invalid hex)");
        }
        v = v << 4 | x.first;
      }
      s = s.substr(n);
      if (!is_bytes && v > 0x0010FFFF) {
        return std::make_tuple("", s,
                               "unable to unescape string"
                               "(value out of bounds)");
      }
      value = v;
      break;
    }

    // 5. Octal escape sequences, must be three digits \[0-3][0-7][0-7]
    case '0':
      [[fallthrough]];
    case '1':
      [[fallthrough]];
    case '2':
      [[fallthrough]];
    case '3': {
      if (s.size() < 2) {
        return std::make_tuple("", s,
                               "unable to unescape octal sequence in string");
      }
      char32_t v = c - '0';
      for (int j = 0; j < 2; ++j) {
        char x = s[j];
        if (x < '0' || x > '7') {
          return std::make_tuple("", s,
                                 "unable to unescape octal sequence "
                                 "in string");
        }
        v = v * 8 + (x - '0');
      }
      if (!is_bytes && v > 0x0010FFFF) {
        return std::make_tuple("", s, "unable to unescape string");
      }
      value = v;
      s = s.substr(2);
      encode = !is_bytes;
    } break;

    // Unknown escape sequence.
    default:
      return std::make_tuple("", s, "unable to unescape string");
  }

  if (value < 0x80 || !encode) {
    char tmp[2] = {(char)value, '\0'};
    return std::make_tuple(std::string(tmp), s, "");
  } else {
    UnicodeText ut;
    ut.push_back(value);
    return std::make_tuple(ut.begin().get_utf8_string(), s, "");
  }
}

// Unescape takes a quoted string, unquotes, and unescapes it.
std::optional<std::string> unescape(const std::string& s, bool is_bytes) {
  // All strings normalize newlines to the \n representation.
  std::string value = absl::StrReplaceAll(s, {{"\r\n", "\n"}, {"\r", "\n"}});

  size_t n = value.size();

  // Nothing to unescape / decode.
  if (n < 2) {
    return std::make_optional(value);
  }

  // Raw string preceded by the 'r|R' prefix.
  bool is_raw_literal = false;
  if (value[0] == 'r' || value[0] == 'R') {
    value.resize(value.size() - 1);
    n = value.size();
    is_raw_literal = true;
  }

  // Quoted string of some form, must have same first and last char.
  if (value[0] != value[n - 1] || (value[0] != '"' && value[0] != '\'')) {
    return std::optional<std::string>();
  }

  // Normalize the multi-line CEL string representation to a standard
  // Google SQL or Go quoted string, as accepted by CEL.
  if (n >= 6) {
    if (absl::StartsWith(value, "'''")) {
      if (!absl::EndsWith(value, "'''")) {
        return std::optional<std::string>();
      }
      value = "\"" + value.substr(3, n - 6) + "\"";
    } else if (absl::StartsWith(value, "\"\"\"")) {
      if (!absl::EndsWith(value, "\"\"\"")) {
        return std::optional<std::string>();
      }
      value = "\"" + value.substr(3, n - 6) + "\"";
    }
    n = value.size();
  }
  value = value.substr(1, n - 2);
  // If there is nothing to escape, then return.
  if (is_raw_literal || (value.find("\\") == std::string::npos)) {
    return value;
  }

  if (is_bytes) {
    // first convert byte values the non-UTF8 way
    std::string new_value;
    for (std::string::size_type i = 0; i < value.size() - 1; ++i) {
      if (value[i] == '\\') {
        if (value[i + 1] == 'x' || value[i + 1] == 'X') {
          if (i > (std::numeric_limits<std::string::size_type>::max() - 3) ||
              i + 3 >= value.size()) {
            return std::optional<std::string>();
          }
          char v = 0;
          for (int j = 2; j <= 3; ++j) {
            auto x = unhex(value[i + j]);
            v = v << 4 | x.first;
          }
          i += 3;
          new_value += v;
        } else if (value[i + 1] == '0' || value[i + 1] == '1' ||
                   value[i + 1] == '2' || value[i + 1] == '3') {
          if (i > (std::numeric_limits<std::string::size_type>::max() - 3) ||
              i + 3 >= value.size()) {
            return std::optional<std::string>();
          }
          char v = value[i + 1] - '0';
          for (int j = 1; j <= 3; ++j) {
            char x = value[i + j];
            if (x < '0' || x > '7') {
              return std::optional<std::string>();
            }
            v = v * 8 + (x - '0');
          }
          i += 3;
          new_value += v;
        } else {
          return std::optional<std::string>();
        }
      } else {
        new_value += value[i];
      }
    }
    value = std::move(new_value);
  }

  std::string unescaped;
  unescaped.reserve(3 * value.size() / 2);
  std::string_view value_sv(value);
  while (!value_sv.empty()) {
    std::tuple<std::string, std::string_view, std::string> c =
        unescape_char(value_sv, is_bytes);
    if (!std::get<2>(c).empty()) {
      return std::optional<std::string>();
    }

    unescaped.append(std::get<0>(c));
    value_sv = std::get<1>(c);
  }
  return std::make_optional(unescaped);
}

std::string escapeAndQuote(std::string_view str) {
  const std::string lowerhex = "0123456789abcdef";

  std::string s;
  for (auto c : str) {
    switch (c) {
      case '\a':
        s.append("\\a");
        break;
      case '\b':
        s.append("\\b");
        break;
      case '\f':
        s.append("\\f");
        break;
      case '\n':
        s.append("\\n");
        break;
      case '\r':
        s.append("\\r");
        break;
      case '\t':
        s.append("\\t");
        break;
      case '\v':
        s.append("\\v");
        break;
      case '"':
        s.append("\\\"");
        break;
      default:
        s += c;
        break;
    }
  }
  return absl::StrFormat("\"%s\"", s);
}

}  // namespace parser
}  // namespace expr
}  // namespace api
}  // namespace google
