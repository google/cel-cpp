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

#include "base/type.h"

#include <cstdlib>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/call_once.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "base/internal/type.h"
#include "internal/reference_counted.h"

namespace cel {

namespace base_internal {

// Implementation of BaseType for simple types. See SimpleTypes below for the
// types being implemented.
class SimpleType final : public BaseType {
 public:
  constexpr SimpleType(Kind kind, absl::string_view name)
      : BaseType(), name_(name), kind_(kind) {}

  ~SimpleType() override {
    // Simple types should live for the lifetime of the process, so destructing
    // them is definetly a bug.
    std::abort();
  }

  Kind kind() const override { return kind_; }

  absl::string_view name() const override { return name_; }

  absl::Span<const Type> parameters() const override { return {}; }

 protected:
  void HashValue(absl::HashState state) const override {
    // cel::Type already adds both kind and name to the hash state, nothing else
    // for us to do.
    static_cast<void>(state);
  }

  bool Equals(const cel::Type& other) const override {
    // cel::Type already checks that the kind and name are equivalent, so at
    // this point the types are the same.
    static_cast<void>(other);
    return true;
  }

 private:
  const absl::string_view name_;
  const Kind kind_;
};

}  // namespace base_internal

namespace {

struct SimpleTypes final {
  constexpr SimpleTypes() = default;

  SimpleTypes(const SimpleTypes&) = delete;

  SimpleTypes(SimpleTypes&&) = delete;

  ~SimpleTypes() = default;

  SimpleTypes& operator=(const SimpleTypes&) = delete;

  SimpleTypes& operator=(SimpleTypes&&) = delete;

  Type error_type;
  Type null_type;
  Type dyn_type;
  Type any_type;
  Type bool_type;
  Type int_type;
  Type uint_type;
  Type double_type;
  Type string_type;
  Type bytes_type;
  Type duration_type;
  Type timestamp_type;
};

ABSL_CONST_INIT absl::once_flag simple_types_once;
ABSL_CONST_INIT SimpleTypes* simple_types = nullptr;

}  // namespace

void Type::Initialize() {
  absl::call_once(simple_types_once, []() {
    ABSL_ASSERT(simple_types == nullptr);
    simple_types = new SimpleTypes();
    simple_types->error_type =
        Type(new base_internal::SimpleType(Kind::kError, "*error*"));
    simple_types->dyn_type =
        Type(new base_internal::SimpleType(Kind::kDyn, "dyn"));
    simple_types->any_type =
        Type(new base_internal::SimpleType(Kind::kAny, "google.protobuf.Any"));
    simple_types->bool_type =
        Type(new base_internal::SimpleType(Kind::kBool, "bool"));
    simple_types->int_type =
        Type(new base_internal::SimpleType(Kind::kInt, "int"));
    simple_types->uint_type =
        Type(new base_internal::SimpleType(Kind::kUint, "uint"));
    simple_types->double_type =
        Type(new base_internal::SimpleType(Kind::kDouble, "double"));
    simple_types->string_type =
        Type(new base_internal::SimpleType(Kind::kString, "string"));
    simple_types->bytes_type =
        Type(new base_internal::SimpleType(Kind::kBytes, "bytes"));
    simple_types->duration_type = Type(new base_internal::SimpleType(
        Kind::kDuration, "google.protobuf.Duration"));
    simple_types->timestamp_type = Type(new base_internal::SimpleType(
        Kind::kTimestamp, "google.protobuf.Timestamp"));
  });
}

const Type& Type::Simple(Kind kind) {
  switch (kind) {
    case Kind::kNullType:
      return Null();
    case Kind::kError:
      return Error();
    case Kind::kBool:
      return Bool();
    case Kind::kInt:
      return Int();
    case Kind::kUint:
      return Uint();
    case Kind::kDouble:
      return Double();
    case Kind::kDuration:
      return Duration();
    case Kind::kTimestamp:
      return Timestamp();
    case Kind::kString:
      return String();
    case Kind::kBytes:
      return Bytes();
    default:
      // We can only get here via memory corruption in cel::Value via
      // cel::base_internal::ValueMetadata, as the the kinds with simple tags
      // are all covered here.
      std::abort();
  }
}

const Type& Type::Null() {
  Initialize();
  return simple_types->null_type;
}

const Type& Type::Error() {
  Initialize();
  return simple_types->error_type;
}

const Type& Type::Dyn() {
  Initialize();
  return simple_types->dyn_type;
}

const Type& Type::Any() {
  Initialize();
  return simple_types->any_type;
}

const Type& Type::Bool() {
  Initialize();
  return simple_types->bool_type;
}

const Type& Type::Int() {
  Initialize();
  return simple_types->int_type;
}

const Type& Type::Uint() {
  Initialize();
  return simple_types->uint_type;
}

const Type& Type::Double() {
  Initialize();
  return simple_types->double_type;
}

const Type& Type::String() {
  Initialize();
  return simple_types->string_type;
}

const Type& Type::Bytes() {
  Initialize();
  return simple_types->bytes_type;
}

const Type& Type::Duration() {
  Initialize();
  return simple_types->duration_type;
}

const Type& Type::Timestamp() {
  Initialize();
  return simple_types->timestamp_type;
}

Type::Type(const Type& other) : impl_(other.impl_) { internal::Ref(impl_); }

Type::Type(Type&& other) : impl_(other.impl_) { other.impl_ = nullptr; }

Type& Type::operator=(const Type& other) {
  if (ABSL_PREDICT_TRUE(this != &other)) {
    internal::Ref(other.impl_);
    internal::Unref(impl_);
    impl_ = other.impl_;
  }
  return *this;
}

Type& Type::operator=(Type&& other) {
  if (ABSL_PREDICT_TRUE(this != &other)) {
    internal::Unref(impl_);
    impl_ = other.impl_;
    other.impl_ = nullptr;
  }
  return *this;
}

bool Type::Equals(const Type& other) const {
  return impl_ == other.impl_ ||
         (kind() == other.kind() && name() == other.name() &&
          // It should not be possible to reach here if impl_ is nullptr.
          impl_->Equals(other));
}

void Type::HashValue(absl::HashState state) const {
  state = absl::HashState::combine(std::move(state), kind(), name());
  if (impl_) {
    impl_->HashValue(std::move(state));
  }
}

}  // namespace cel
