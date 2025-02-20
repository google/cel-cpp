// Copyright 2025 Google LLC
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

// IWYU pragma: private, include "common/value.h"
// IWYU pragma: friend "common/value.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_BYTES_VALUE_INPUT_STREAM_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_BYTES_VALUE_INPUT_STREAM_H_

#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "absl/utility/utility.h"
#include "common/values/bytes_value.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace cel {

class BytesValueInputStream final : public google::protobuf::io::ZeroCopyInputStream {
 public:
  explicit BytesValueInputStream(
      absl::Nonnull<const BytesValue*> value ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    Construct(value);
  }

  ~BytesValueInputStream() override { AsVariant().~variant(); }

  bool Next(const void** data, int* size) override {
    return absl::visit(
        [&data, &size](auto& alternative) -> bool {
          return alternative.Next(data, size);
        },
        AsVariant());
  }

  void BackUp(int count) override {
    absl::visit(
        [&count](auto& alternative) -> void { alternative.BackUp(count); },
        AsVariant());
  }

  bool Skip(int count) override {
    return absl::visit(
        [&count](auto& alternative) -> bool { return alternative.Skip(count); },
        AsVariant());
  }

  int64_t ByteCount() const override {
    return absl::visit(
        [](const auto& alternative) -> int64_t {
          return alternative.ByteCount();
        },
        AsVariant());
  }

  bool ReadCord(absl::Cord* cord, int count) override {
    return absl::visit(
        [&cord, &count](auto& alternative) -> bool {
          return alternative.ReadCord(cord, count);
        },
        AsVariant());
  }

 private:
  using Variant =
      absl::variant<google::protobuf::io::ArrayInputStream, google::protobuf::io::CordInputStream>;

  void Construct(absl::Nonnull<const BytesValue*> value) {
    ABSL_DCHECK(value != nullptr);
    if (value->value_.header_.is_cord) {
      ::new (static_cast<void*>(&impl_[0]))
          Variant(absl::in_place_type<google::protobuf::io::CordInputStream>,
                  value->value_.cord_ptr());
    } else {
      absl::string_view string = value->value_.AsStringView();
      ABSL_DCHECK_LE(string.size(),
                     static_cast<size_t>(std::numeric_limits<int>::max()));
      ::new (static_cast<void*>(&impl_[0]))
          Variant(absl::in_place_type<google::protobuf::io::ArrayInputStream>,
                  string.data(), static_cast<int>(string.size()));
    }
  }

  void Destruct() { AsVariant().~variant(); }

  Variant& AsVariant() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return *std::launder(reinterpret_cast<Variant*>(&impl_[0]));
  }

  const Variant& AsVariant() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return *std::launder(reinterpret_cast<const Variant*>(&impl_[0]));
  }

  alignas(Variant) char impl_[sizeof(Variant)];
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_BYTES_VALUE_INPUT_STREAM_H_
