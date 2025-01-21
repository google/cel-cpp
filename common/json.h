// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_JSON_H_
#define THIRD_PARTY_CEL_CPP_COMMON_JSON_H_

#include <cstdint>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

// Maximum `int64_t` value that can be represented as `double` without losing
// data.
inline constexpr int64_t kJsonMaxInt = (int64_t{1} << 53) - 1;
// Minimum `int64_t` value that can be represented as `double` without losing
// data.
inline constexpr int64_t kJsonMinInt = -kJsonMaxInt;

// Maximum `uint64_t` value that can be represented as `double` without losing
// data.
inline constexpr uint64_t kJsonMaxUint = (uint64_t{1} << 53) - 1;

class AnyToJsonConverter {
 public:
  virtual ~AnyToJsonConverter() = default;

  virtual absl::Nullable<const google::protobuf::DescriptorPool*> descriptor_pool()
      const {
    return nullptr;
  }

  virtual absl::Nullable<google::protobuf::MessageFactory*> message_factory() const = 0;
};

inline std::pair<absl::Nonnull<const google::protobuf::DescriptorPool*>,
                 absl::Nonnull<google::protobuf::MessageFactory*>>
GetDescriptorPoolAndMessageFactory(
    AnyToJsonConverter& converter ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  const auto* descriptor_pool = converter.descriptor_pool();
  auto* message_factory = converter.message_factory();
  if (descriptor_pool == nullptr) {
    descriptor_pool = message.GetDescriptor()->file()->pool();
    if (message_factory == nullptr) {
      message_factory = message.GetReflection()->GetMessageFactory();
    }
  }
  return std::pair{descriptor_pool, message_factory};
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_JSON_H_
