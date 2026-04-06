// Copyright 2026 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_VALIDATOR_COMPREHENSION_NESTING_VALIDATOR_H_
#define THIRD_PARTY_CEL_CPP_VALIDATOR_COMPREHENSION_NESTING_VALIDATOR_H_

#include "validator/validator.h"

namespace cel {

// Returns a `Validation` that checks that comprehensions are not nested beyond
// the specified limit.
//
// Comprehensions with an empty iteration range (e.g. `cel.bind`) do not count
// towards the nesting limit.
Validation ComprehensionNestingLimitValidator(int limit);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_VALIDATOR_COMPREHENSION_NESTING_VALIDATOR_H_
