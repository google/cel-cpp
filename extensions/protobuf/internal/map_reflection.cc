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

#include "extensions/protobuf/internal/map_reflection.h"

namespace google::protobuf::expr {

class CelMapReflectionFriend final {
 public:
  static bool LookupMapValue(const Reflection& reflection,
                             const Message& message,
                             const FieldDescriptor& field, const MapKey& key,
                             MapValueConstRef* value) {
    return reflection.LookupMapValue(message, &field, key, value);
  }

  static bool ContainsMapKey(const Reflection& reflection,
                             const Message& message,
                             const FieldDescriptor& field, const MapKey& key) {
    return reflection.ContainsMapKey(message, &field, key);
  }
};

}  // namespace google::protobuf::expr

namespace cel::extensions::protobuf_internal {

bool LookupMapValue(const google::protobuf::Reflection& reflection,
                    const google::protobuf::Message& message,
                    const google::protobuf::FieldDescriptor& field,
                    const google::protobuf::MapKey& key,
                    google::protobuf::MapValueConstRef* value) {
  return google::protobuf::expr::CelMapReflectionFriend::LookupMapValue(
      reflection, message, field, key, value);
}

bool ContainsMapKey(const google::protobuf::Reflection& reflection,
                    const google::protobuf::Message& message,
                    const google::protobuf::FieldDescriptor& field,
                    const google::protobuf::MapKey& key) {
  return google::protobuf::expr::CelMapReflectionFriend::ContainsMapKey(
      reflection, message, field, key);
}

}  // namespace cel::extensions::protobuf_internal
