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

#ifndef THIRD_PARTY_CEL_CPP_VALIDATOR_TIMESTAMP_LITERAL_VALIDATOR_H_
#define THIRD_PARTY_CEL_CPP_VALIDATOR_TIMESTAMP_LITERAL_VALIDATOR_H_

#include "validator/validator.h"
namespace cel {

// Returns a `Validation` that checks timestamp literals are valid for CEL.
const Validation& TimestampLiteralValidator();

// Returns a `Validation` that checks duration literals are valid for CEL.
const Validation& DurationLiteralValidator();

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_VALIDATOR_TIMESTAMP_LITERAL_VALIDATOR_H_
