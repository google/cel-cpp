// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_BASE_VALUE_H_
#define THIRD_PARTY_CEL_CPP_BASE_VALUE_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "base/handle.h"
#include "base/internal/value.pre.h"  // IWYU pragma: export
#include "base/kind.h"
#include "base/memory_manager.h"
#include "base/type.h"

namespace cel {

class Value;
class NullValue;
class ErrorValue;
class BoolValue;
class IntValue;
class UintValue;
class DoubleValue;
class BytesValue;
class StringValue;
class DurationValue;
class TimestampValue;
class EnumValue;
class StructValue;
class ListValue;
class MapValue;
class TypeValue;
class ValueFactory;

namespace internal {
template <typename T>
class NoDestructor;
}

namespace interop_internal {
base_internal::StringValueRep GetStringValueRep(
    const Persistent<const StringValue>& value);
base_internal::BytesValueRep GetBytesValueRep(
    const Persistent<const BytesValue>& value);
}  // namespace interop_internal

// A representation of a CEL value that enables reflection and introspection of
// values.
class Value : public base_internal::Resource {
 public:
  // Returns the type of the value. If you only need the kind, prefer `kind()`.
  virtual Persistent<const Type> type() const = 0;

  // Returns the kind of the value. This is equivalent to `type().kind()` but
  // faster in many scenarios. As such it should be preffered when only the kind
  // is required.
  virtual Kind kind() const { return type()->kind(); }

  virtual std::string DebugString() const = 0;

  // Called by base_internal::ValueHandleBase.
  // Note GCC does not consider a friend member as a member of a friend.
  virtual bool Equals(const Value& other) const = 0;

  // Called by base_internal::ValueHandleBase.
  // Note GCC does not consider a friend member as a member of a friend.
  virtual void HashValue(absl::HashState state) const = 0;

 private:
  friend class NullValue;
  friend class ErrorValue;
  friend class BoolValue;
  friend class IntValue;
  friend class UintValue;
  friend class DoubleValue;
  friend class BytesValue;
  friend class StringValue;
  friend class DurationValue;
  friend class TimestampValue;
  friend class EnumValue;
  friend class StructValue;
  friend class ListValue;
  friend class MapValue;
  friend class TypeValue;
  friend class base_internal::ValueHandleBase;
  friend class base_internal::StringBytesValue;
  friend class base_internal::ExternalDataBytesValue;
  friend class base_internal::StringStringValue;
  friend class base_internal::ExternalDataStringValue;

  Value() = default;
  Value(const Value&) = default;
  Value(Value&&) = default;

  // Called by base_internal::ValueHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Value& value) { return true; }

  // For non-inlined values that are reference counted, this is the result of
  // `sizeof` and `alignof` for the most derived class.
  std::pair<size_t, size_t> SizeAndAlignment() const override;

  // Expose to some value implementations using friendship.
  using base_internal::Resource::Ref;
  using base_internal::Resource::Unref;

  // Called by base_internal::ValueHandleBase for inlined values.
  virtual void CopyTo(Value& address) const;

  // Called by base_internal::ValueHandleBase for inlined values.
  virtual void MoveTo(Value& address);
};

}  // namespace cel

// value.pre.h forward declares types so they can be friended above. The types
// themselves need to be defined after everything else as they need to access or
// derive from the above types. We do this in value.post.h to avoid mudying this
// header and making it difficult to read.
#include "base/internal/value.post.h"  // IWYU pragma: export

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUE_H_
