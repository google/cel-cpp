#include "base/unilib.h"

#include "flatbuffers/util.h"

namespace UniLib {

// Detects whether a string is valid UTF-8.
bool IsStructurallyValid(absl::string_view str) {
  if (str.size() == 0) {
    return true;
  }
  const char *s = &str[0];
  const char *const sEnd = s + str.length();
  while (s < sEnd) {
    if (flatbuffers::FromUTF8(&s) < 0) {
      return false;
    }
  }
  return true;
}

}  // namespace UniLib
