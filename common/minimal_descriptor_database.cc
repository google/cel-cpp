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

#include "common/minimal_descriptor_database.h"

#include "absl/base/nullability.h"
#include "internal/minimal_descriptor_database.h"
#include "google/protobuf/descriptor_database.h"

namespace cel {

google::protobuf::DescriptorDatabase* ABSL_NONNULL GetMinimalDescriptorDatabase() {
  return internal::GetMinimalDescriptorDatabase();
}

}  // namespace cel
