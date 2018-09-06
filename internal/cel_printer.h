/**
 * Helper classes to converts native value into CEL expressions.
 */

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_CEL_PRINTER_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_CEL_PRINTER_H_

#include <sstream>

#include "absl/strings/escaping.h"
#include "absl/time/time.h"
#include "internal/specialize.h"
#include "internal/types.h"
#include "internal/visitor_util.h"

namespace google {
namespace api {
namespace expr {
namespace internal {

// Print specific overload helpers.
template <int B, typename... Args>
using print_if = specialize_if<B, std::string, Args...>;
template <typename B, typename... Args>
using print_ift = specialize_ift<B, std::string, Args...>;

// Helper to print a string value without quotes/escaping.
struct RawString {
  std::string name;
  const std::string& ToString() const { return name; }
};

/**
 * Printer for all scalar types (null, bool, int, unit, double, std::string).
 */
struct ScalarPrinter {
  inline std::string operator()(std::nullptr_t) { return "null"; }
  inline std::string operator()(bool value) {
    return value ? "true" : "false";
  }
  std::string operator()(absl::Time value);
  std::string operator()(absl::Duration value);

  // Print int directly.
  template <typename T>
  print_ift<is_int<T>> operator()(T value) {
    return absl::StrCat(value);
  }

  // Print uint directly with a trailing 'u'.
  template <typename T>
  print_ift<is_uint<T>> operator()(T value) {
    return absl::StrCat(value, "u");
  }

  // Print float types directly and add a trailing '.0' if necessary.
  template <typename T>
  print_ift<is_float<T>> operator()(T value) {
    T ipart;
    if (std::isfinite(value) && std::modf(value, &ipart) == 0.0) {
      return absl::StrCat(value, ".0");
    }
    return absl::StrCat(value);
  }

  // Quote and escape any 'string' type.
  template <typename T>
  print_ift<is_string<T>> operator()(T&& value) {
    return absl::StrCat("\"", absl::CEscape(value), "\"");
  }
};

/**
 * A printer that forwards to functions on the value.
 */
struct ForwardingPrinter {
  // If the value defines a ToString function, call it.
  template <typename T>
  specialize_ifd<std::string, decltype(inst_of<T&&>().ToString())> operator()(
      T&& value) {
    return std::string(value.ToString());
  }

  // If the value defines a ToDebugString function, call it.
  template <typename T>
  specialize_ifd<std::string, decltype(inst_of<T&&>().ToDebugString())> operator()(
      T&& value) {
    return std::string(value.ToDebugString());
  }
};

/**
 * A printer that can print all CelValue values.
 */
struct CelPrinter : OrderedVisitor<ScalarPrinter, ForwardingPrinter> {};

/**
 * The type of key used in a sequence.
 */
enum KeyType {
  kNoKey,
  kValueKey,
  kIdentKey,
};

/**
 * The base join policy for use with sequence printers.
 */
struct BaseJoinPolicy {
  static constexpr const absl::string_view kValueDelim = ", ";
  static constexpr const KeyType kKeyType = kNoKey;
};

/**
 * Produces: <name>[<value>, <value>, ...]
 */
struct ListJoinPolicy : BaseJoinPolicy {
  static constexpr const absl::string_view kStart = "[";
  static constexpr const absl::string_view kEnd = "]";
};

/**
 * Produces: <name>{<value>, <value>, ...}
 */
struct SetJoinPolicy : BaseJoinPolicy {
  static constexpr const absl::string_view kStart = "{";
  static constexpr const absl::string_view kEnd = "}";
};

/**
 * Produces: <name>(<value>, <value>, ...)
 */
struct CallJoinPolicy : BaseJoinPolicy {
  static constexpr const absl::string_view kStart = "(";
  static constexpr const absl::string_view kEnd = ")";
};

/**
 * Produces: <name>{<key>: <value>, <key>: <value>, ...}
 */
struct MapJoinPolicy : SetJoinPolicy {
  static constexpr const absl::string_view kKeyDelim = ": ";
  static constexpr KeyType kKeyType = kValueKey;
};

/**
 * Produces: <name>{<key>=<value>, <key>=<value>, ...}
 */
struct ObjectJoinPolicy : SetJoinPolicy {
  static constexpr const absl::string_view kKeyDelim = "=";
  static constexpr const KeyType kKeyType = kIdentKey;
};

// Join policy specific overload helpers.
template <typename JoinPolicy, typename... Args>
using print_if_no_key = print_if<!JoinPolicy::kKeyType, Args...>;
template <typename JoinPolicy, typename... Args>
using print_if_has_key = print_if<JoinPolicy::kKeyType, Args...>;
template <typename JoinPolicy, typename... Args>
using print_if_value_key = print_if<JoinPolicy::kKeyType == kValueKey, Args...>;
template <typename JoinPolicy, typename... Args>
using print_if_ident_key = print_if<JoinPolicy::kKeyType == kIdentKey, Args...>;

/**
 * A printer for entries in a sequence.
 */
template <typename JoinPolicy>
struct EntryPrinter {
  CelPrinter value_printer;

  // Print an entry with a value key.
  template <typename K, typename V>
  print_if_value_key<JoinPolicy, V> operator()(K&& key, V&& value) {
    return absl::StrCat(value_printer(std::forward<K>(key)),
                        JoinPolicy::kKeyDelim,
                        value_printer(std::forward<V>(value)));
  }

  // Print an entry with a ident key.
  template <typename V>
  print_if_ident_key<JoinPolicy, V> operator()(absl::string_view ident_key,
                                               V&& value) {
    if (ident_key.empty()) {
      return value_printer(std::forward<V>(value));
    }
    return absl::StrCat(ident_key, JoinPolicy::kKeyDelim,
                        value_printer(std::forward<V>(value)));
  }

  template <typename T>
  print_if_no_key<JoinPolicy, T> operator()(T&& entry) {
    // Pass through.
    return value_printer(std::forward<T>(entry));
  }

  // Forward first and second to the proper overload.
  template <typename T>
  print_if_has_key<JoinPolicy, T> operator()(T&& entry) {
    return (*this)(std::forward<T>(entry).first, std::forward<T>(entry).second);
  }
};

/**
 * A sequence printer for standard containers.
 */
template <typename JoinPolicy>
struct SequencePrinter {
  EntryPrinter<JoinPolicy> entry_printer;

  template <typename T>
  std::string operator()(absl::string_view name, T&& value) {
    std::string result;
    absl::StrAppend(&result, name, JoinPolicy::kStart);
    auto itr = value.begin();
    if (itr != value.end()) {
      absl::StrAppend(&result, entry_printer(*itr));
      while (++itr != value.end()) {
        absl::StrAppend(&result, JoinPolicy::kValueDelim, entry_printer(*itr));
      }
    }
    absl::StrAppend(&result, JoinPolicy::kEnd);
    return result;
  }
};

/**
 * A sequence printer for sequences with variable types, known at compile time.
 */
template <typename JoinPolicy>
struct VarSequencePrinter {
  EntryPrinter<JoinPolicy> entry_printer;

  template <typename... Args>
  std::string operator()(absl::string_view name, Args&&... args) {
    std::string result;
    absl::StrAppend(&result, name, JoinPolicy::kStart);
    PrintArgs(&result, std::forward<Args>(args)...);
    absl::StrAppend(&result, JoinPolicy::kEnd);
    return result;
  }

 private:
  // No args.
  void PrintArgs(std::string*) {}

  // Args for a non-keyed collection.
  template <typename V, typename... Args>
  void PrintArgs(print_if_no_key<JoinPolicy, V>* result, V&& value,
                 Args&&... args) {
    absl::StrAppend(result, entry_printer(std::forward<V>(value)));
    if (!args_empty<Args...>::value) {
      absl::StrAppend(result, JoinPolicy::kValueDelim);
      PrintArgs(result, std::forward<Args>(args)...);
    }
  }

  // Args for a keyed collection.
  template <typename K, typename V, typename... Args>
  void PrintArgs(print_if_has_key<JoinPolicy, V>* result, K&& key, V&& value,
                 Args&&... args) {
    absl::StrAppend(
        result, entry_printer(std::forward<K>(key), std::forward<V>(value)));
    if (!args_empty<Args...>::value) {
      absl::StrAppend(result, JoinPolicy::kValueDelim);
      PrintArgs(result, std::forward<Args>(args)...);
    }
  }
};

/**
 * A sequence builder for all types of sequences.
 */
template <typename JoinPolicy>
class SequenceBuilder {
 public:
  template <typename... Args>
  void Add(Args&&... args) {
    absl::StrAppend(&result_, entry_printer_(std::forward<Args>(args)...),
                    JoinPolicy::kValueDelim);
  }

  std::string Build(absl::string_view name = "") {
    absl::string_view result(result_);
    if (!result.empty()) {
      result =
          result.substr(0, result.size() - JoinPolicy::kValueDelim.length());
    }
    return absl::StrCat(name, JoinPolicy::kStart, result, JoinPolicy::kEnd);
  }

 private:
  std::string result_;
  EntryPrinter<JoinPolicy> entry_printer_;
};

/**
 * Helper function to print a single value.
 */
template <typename T>
std::string ToString(T&& value) {
  CelPrinter printer;
  return printer(std::forward<T>(value));
}

/**
 * Helper function to print a list value.
 */
template <typename T>
std::string ToListString(T&& value) {
  SequencePrinter<ListJoinPolicy> printer;
  return printer("", std::forward<T>(value));
}

// Helper overload to make initializer list literals work.
template <typename T = int>
std::string ToListString(std::initializer_list<T>&& value) {
  SequencePrinter<ListJoinPolicy> printer;
  return printer("", std::forward<std::initializer_list<T>>(value));
}

/**
 * Helper function to print a map value.
 */
template <typename T>
std::string ToMapString(T&& value) {
  SequencePrinter<MapJoinPolicy> printer;
  return printer("", std::forward<T>(value));
}

// Helper overload to make initializer list literals work.
template <typename T = std::pair<int, int>>
std::string ToMapString(std::initializer_list<T>&& value) {
  SequencePrinter<MapJoinPolicy> printer;
  return printer("", std::forward<std::initializer_list<T>>(value));
}

/**
 * Helper function to print a call to a function.
 */
template <typename... Args>
std::string ToCallString(absl::string_view name, Args&&... args) {
  VarSequencePrinter<CallJoinPolicy> printer;
  return printer(name, std::forward<Args>(args)...);
}

/**
 * Helper function to print object creation.
 */
template <typename... Args>
std::string ToObjectString(absl::string_view name, Args&&... args) {
  VarSequencePrinter<ObjectJoinPolicy> printer;
  return printer(name, std::forward<Args>(args)...);
}

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_CEL_PRINTER_H_
