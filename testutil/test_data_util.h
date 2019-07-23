#ifndef THIRD_PARTY_CEL_CPP_TESTUTIL_TEST_DATA_UTIL_H_
#define THIRD_PARTY_CEL_CPP_TESTUTIL_TEST_DATA_UTIL_H_

#include "google/api/expr/v1beta1/eval.pb.h"
#include "google/api/expr/v1beta1/value.pb.h"
#include "google/protobuf/message.h"
#include "google/protobuf/util/message_differencer.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "common/type.h"
#include "internal/types.h"
#include "testutil/test_data_util.h"
#include "testdata/test_value.pb.h"

namespace google {
namespace api {
namespace expr {

namespace v1beta1 {

/**
 * Initializes a message differencer to support value equivelance checks
 * for v1beta1::Value.
 */
void InitValueDifferencer(google::protobuf::util::MessageDifferencer* differencer);

}  // namespace v1beta1

namespace testutil {

/**
 * Returns the set of all pairs of different test values.
 */
std::vector<std::pair<testdata::TestValue, testdata::TestValue>> AllPairs(
    const testdata::TestValues& value_cases);

/** Converts a nullptr to a TestValue. */
testdata::TestValue NewValue(std::nullptr_t value);
/** Converts a bool to a TestValue. */
testdata::TestValue NewValue(bool value);
/** Converts a double to a TestValue. */
testdata::TestValue NewValue(double value, absl::string_view name = "");

/** Converts a int64_t to a TestValue. */
testdata::TestValue NewValue(int64_t value, absl::string_view name = "");

/** Converts a unit64_t to a TestValue. */
testdata::TestValue NewValue(uint64_t value, absl::string_view name = "");

/** Converts a string_view to a TestValue. */
testdata::TestValue NewValue(absl::string_view value,
                             absl::string_view name = "");
/** Converts a std::string literal to a TestValue. */
testdata::TestValue NewValue(const char* value, absl::string_view name = "");
/** Converts a nullptr to a TestValue. */
testdata::TestValue NewValue(const google::protobuf::Message& value,
                             absl::string_view name = "");
/** Converts a duration to a TestValue. */
testdata::TestValue NewValue(absl::Duration value, absl::string_view name = "");
/** Converts a time to a TestValue. */
testdata::TestValue NewValue(absl::Time value, absl::string_view name = "");

/** Converts a string_view of bytes to a TestValue. */
testdata::TestValue NewBytesValue(absl::string_view value,
                                  absl::string_view name = "");

testdata::TestValue NewValue(const expr::common::Type& type);

// Helpers to remove overload ambiguity.
template <typename T>
expr::internal::specialize_ift<expr::internal::is_float<T>, testdata::TestValue>
NewValue(T&& value, absl::string_view name = "") {
  return NewValue(static_cast<double>(value), name);
}
template <typename T>
expr::internal::specialize_ift<expr::internal::is_int<T>, testdata::TestValue>
NewValue(T&& value, absl::string_view name = "") {
  return NewValue(static_cast<int64_t>(value), name);
}
template <typename T>
expr::internal::specialize_ift<expr::internal::is_uint<T>, testdata::TestValue>
NewValue(T&& value, absl::string_view name = "") {
  return NewValue(static_cast<uint64_t>(value), name);
}

namespace internal {

void AppendToList(const testdata::TestValue& test_value,
                  testdata::TestValue* list_value);

void AppendToList(testdata::TestValue* list_value);
template <typename T, typename... Args>
void AppendToList(testdata::TestValue* list_value, T&& next,
                  Args&&... remaining) {
  AppendToList(NewValue(std::forward<T>(next)), list_value);
  AppendToList(list_value, std::forward<Args>(remaining)...);
}

void AddToMap(const testdata::TestValue& key, const testdata::TestValue& value,
              testdata::TestValue* map_value);

void AddToMap(testdata::TestValue* map_value);

template <typename K, typename V, typename... Args>
void AddToMap(testdata::TestValue* map_value, K&& next_key, V&& next_value,
              Args&&... remaining) {
  AddToMap(NewValue(std::forward<K>(next_key)),
           NewValue(std::forward<V>(next_value)), map_value);
  AddToMap(map_value, std::forward<Args>(remaining)...);
}

void StartList(testdata::TestValue* list_value);
void EndList(testdata::TestValue* list_value);
void StartMap(testdata::TestValue* map_value);
void EndMap(testdata::TestValue* map_value);

}  // namespace internal

/**
 * Converts arguments into a list TestValue. For example:
 *
 *     TestValue list = NewListValue("elem1", "elem2", 3, 4.0, ...);
 */
template <typename... Args>
testdata::TestValue NewListValue(Args&&... values) {
  testdata::TestValue value;
  value.set_name("list");

  internal::StartList(&value);
  internal::AppendToList(&value, std::forward<Args>(values)...);
  internal::EndList(&value);
  return value;
}

/**
 * Converts arguments into a map TestValue. For example:
 *
 *     TestValue map = NewMapValue("key1", "value1", 2, 2.0, ...);
 */
template <typename... Args>
testdata::TestValue NewMapValue(Args&&... values) {
  testdata::TestValue value;
  value.set_name("map");
  internal::StartMap(&value);
  internal::AddToMap(&value, std::forward<Args>(values)...);
  internal::EndMap(&value);
  return value;
}

/** Creates a new test value with the given name. */
testdata::TestValue WithName(testdata::TestValue value, absl::string_view name);

/** Merges to equivalent test values into a single value. */
testdata::TestValue Merge(const absl::Span<const testdata::TestValue>& values,
                          absl::string_view name = "");

}  // namespace testutil
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_TESTUTIL_TEST_DATA_UTIL_H_
