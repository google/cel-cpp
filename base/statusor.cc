/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "base/statusor.h"

#include <ostream>

#include "base/status.h"

namespace cel_base {

namespace statusor_internal {

void Helper::HandleInvalidStatusCtorArg(Status* status) {
  const char* kMessage =
      "An OK status is not a valid constructor argument to StatusOr<T>";
  ABSL_RAW_CHECK(false, kMessage);
  // In optimized builds, we will fall back to ::util::error::INTERNAL.
  *status = Status(StatusCode::kInternal, kMessage);
}

void Helper::Crash(const Status&) {
  ABSL_RAW_CHECK(false, "Attempting to fetch value instead of handling error");
  abort();
}

}  // namespace statusor_internal

}  // namespace cel_base
