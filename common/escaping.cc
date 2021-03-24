#include "common/escaping.h"

#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"

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

// Write the characters from the first code point into output, which must be at
// least 4 bytes long. Return the number of bytes written.
inline int get_utf8(absl::string_view s, char* buffer) {
  buffer[0] = s[0];
  if (static_cast<uint8_t>(s[0]) < 0x80 || s.size() < 2) return 1;
  buffer[1] = s[1];
  if (static_cast<uint8_t>(s[0]) < 0xE0 || s.size() < 3) return 2;
  buffer[2] = s[2];
  if (static_cast<uint8_t>(s[0]) < 0xF0 || s.size() < 4) return 3;
  buffer[3] = s[3];
  return 4;
}

// Write UTF-8 encoding into a buffer, which must be at least 4 bytes long.
// Return the number of bytes written.
inline int encode_utf8(char* buffer, char32_t utf8_char) {
  if (utf8_char <= 0x7F) {
    *buffer = static_cast<char>(utf8_char);
    return 1;
  } else if (utf8_char <= 0x7FF) {
    buffer[1] = 0x80 | (utf8_char & 0x3F);
    utf8_char >>= 6;
    buffer[0] = 0xC0 | utf8_char;
    return 2;
  } else if (utf8_char <= 0xFFFF) {
    buffer[2] = 0x80 | (utf8_char & 0x3F);
    utf8_char >>= 6;
    buffer[1] = 0x80 | (utf8_char & 0x3F);
    utf8_char >>= 6;
    buffer[0] = 0xE0 | utf8_char;
    return 3;
  } else {
    buffer[3] = 0x80 | (utf8_char & 0x3F);
    utf8_char >>= 6;
    buffer[2] = 0x80 | (utf8_char & 0x3F);
    utf8_char >>= 6;
    buffer[1] = 0x80 | (utf8_char & 0x3F);
    utf8_char >>= 6;
    buffer[0] = 0xF0 | utf8_char;
    return 4;
  }
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
inline std::tuple<std::string, absl::string_view, std::string> unescape_char(
    absl::string_view s, bool is_bytes) {
  char c = s[0];

  // 1. Character is not an escape sequence.
  if (static_cast<uint8_t>(c) >= 0x80 && !is_bytes) {
    char tmp[5];
    int len = get_utf8(s, tmp);
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
      if (static_cast<int>(s.size()) < n) {
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
    char tmp[2] = {static_cast<char>(value), '\0'};
    return std::make_tuple(std::string(tmp), s, "");
  } else {
    char tmp[5];
    int len = encode_utf8(tmp, value);
    tmp[len] = '\0';
    return std::make_tuple(std::string(tmp), s, "");
  }
}

// Unescape takes a quoted string, unquotes, and unescapes it.
absl::optional<std::string> unescape(const std::string& s, bool is_bytes) {
  // All strings normalize newlines to the \n representation.
  std::string value = absl::StrReplaceAll(s, {{"\r\n", "\n"}, {"\r", "\n"}});

  size_t n = value.size();

  // Nothing to unescape / decode.
  if (n < 2) {
    return value;
  }

  // Raw string preceded by the 'r|R' prefix.
  bool is_raw_literal = false;
  if (value[0] == 'r' || value[0] == 'R') {
    value = value.substr(1, n - 1);
    n = value.size();
    is_raw_literal = true;
  }

  // Quoted string of some form, must have same first and last char.
  if (value[0] != value[n - 1] || (value[0] != '"' && value[0] != '\'')) {
    return absl::optional<std::string>();
  }

  // Normalize the multi-line CEL string representation to a standard
  // Google SQL or Go quoted string, as accepted by CEL.
  if (n >= 6) {
    if (absl::StartsWith(value, "'''")) {
      if (!absl::EndsWith(value, "'''")) {
        return absl::optional<std::string>();
      }
      value = "\"" + value.substr(3, n - 6) + "\"";
    } else if (absl::StartsWith(value, "\"\"\"")) {
      if (!absl::EndsWith(value, "\"\"\"")) {
        return absl::optional<std::string>();
      }
      value = "\"" + value.substr(3, n - 6) + "\"";
    }
    n = value.size();
  }
  value = value.substr(1, n - 2);
  // If there is nothing to escape, then return.
  if (is_raw_literal || (!absl::StrContains(value, '\\'))) {
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
            return absl::optional<std::string>();
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
            return absl::optional<std::string>();
          }
          char v = value[i + 1] - '0';
          for (int j = 1; j <= 3; ++j) {
            char x = value[i + j];
            if (x < '0' || x > '7') {
              return absl::optional<std::string>();
            }
            v = v * 8 + (x - '0');
          }
          i += 3;
          new_value += v;
        } else {
          return absl::optional<std::string>();
        }
      } else {
        new_value += value[i];
      }
    }
    value = std::move(new_value);
  }

  std::string unescaped;
  unescaped.reserve(3 * value.size() / 2);
  absl::string_view value_sv(value);
  while (!value_sv.empty()) {
    std::tuple<std::string, absl::string_view, std::string> c =
        unescape_char(value_sv, is_bytes);
    if (!std::get<2>(c).empty()) {
      return absl::optional<std::string>();
    }

    unescaped.append(std::get<0>(c));
    value_sv = std::get<1>(c);
  }
  return unescaped;
}

std::string escapeAndQuote(absl::string_view str) {
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
