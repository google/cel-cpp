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

#include "internal/proto_wire.h"

namespace cel::internal {

bool SkipLengthValue(absl::Cord& data, ProtoWireType type) {
  switch (type) {
    case ProtoWireType::kVarint:
      if (auto result = VarintDecode<uint64_t>(data);
          ABSL_PREDICT_TRUE(result.has_value())) {
        data.RemovePrefix(result->size_bytes);
        return true;
      }
      return false;
    case ProtoWireType::kFixed64:
      if (ABSL_PREDICT_FALSE(data.size() < 8)) {
        return false;
      }
      data.RemovePrefix(8);
      return true;
    case ProtoWireType::kLengthDelimited:
      if (auto result = VarintDecode<uint32_t>(data);
          ABSL_PREDICT_TRUE(result.has_value())) {
        if (ABSL_PREDICT_FALSE(data.size() - result->size_bytes >=
                               result->value)) {
          data.RemovePrefix(result->size_bytes + result->value);
          return true;
        }
      }
      return false;
    case ProtoWireType::kStartGroup:
      ABSL_FALLTHROUGH_INTENDED;
    case ProtoWireType::kEndGroup:
      return false;
    case ProtoWireType::kFixed32:
      if (ABSL_PREDICT_FALSE(data.size() < 4)) {
        return false;
      }
      data.RemovePrefix(4);
      return true;
  }
}

}  // namespace cel::internal
