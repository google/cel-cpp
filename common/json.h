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

#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace cel {

class JsonArrayMutator;
class JsonObjectMutator;

// Maximum `int64_t` value that can be represented as `double` without losing
// data.
inline constexpr int64_t kJsonMaxInt = (int64_t{1} << 53) - 1;
// Minimum `int64_t` value that can be represented as `double` without losing
// data.
inline constexpr int64_t kJsonMinInt = -kJsonMaxInt;

// Maximum `uint64_t` value that can be represented as `double` without losing
// data.
inline constexpr uint64_t kJsonMaxUint = (uint64_t{1} << 53) - 1;

// JsonMutator is an abstract interface for building JSON values. It is used to
// facilitate converting values to JSON.
class JsonMutator {
 public:
  virtual ~JsonMutator() = default;

  virtual absl::Status SetNull() = 0;

  virtual absl::Status SetBool(bool value) = 0;

  virtual absl::Status SetNumber(double value) = 0;

  absl::Status SetInt(int64_t value) {
    if (value < kJsonMinInt || value > kJsonMaxInt) {
      return SetString(absl::StrCat(value));
    }
    return SetNumber(static_cast<double>(value));
  }

  absl::Status SetUint(uint64_t value) {
    if (value > kJsonMaxUint) {
      return SetString(absl::StrCat(value));
    }
    return SetNumber(static_cast<double>(value));
  }

  virtual absl::Status SetString(absl::string_view value) = 0;

  virtual absl::Status SetString(const absl::Cord& value) = 0;

  absl::Status SetBytes(absl::string_view value) {
    return SetString(absl::Base64Escape(value));
  }

  virtual absl::Status SetArray(
      absl::FunctionRef<absl::Status(JsonArrayMutator&)> mutator) = 0;

  virtual absl::Status SetObject(
      absl::FunctionRef<absl::Status(JsonObjectMutator&)> mutator) = 0;
};

// `JsonObjectMutator` is an abstract interface for building JSON objects. It is
// used to facilitate converting values to JSON.
class JsonObjectMutator {
 public:
  virtual ~JsonObjectMutator() = default;

  virtual absl::Status AddField(
      absl::string_view name,
      absl::FunctionRef<absl::Status(JsonMutator&)> mutator) = 0;

  virtual absl::Status AddNullField(absl::string_view name) {
    return AddField(name,
                    [](JsonMutator& mutator) { return mutator.SetNull(); });
  }

  virtual absl::Status AddBoolField(absl::string_view name, bool value) {
    return AddField(
        name, [value](JsonMutator& mutator) { return mutator.SetBool(value); });
  }

  virtual absl::Status AddNumberField(absl::string_view name, double value) {
    return AddField(name, [value](JsonMutator& mutator) {
      return mutator.SetNumber(value);
    });
  }

  virtual absl::Status AddIntField(absl::string_view name, int64_t value) {
    return AddField(
        name, [value](JsonMutator& mutator) { return mutator.SetInt(value); });
  }

  virtual absl::Status AddUintField(absl::string_view name, uint64_t value) {
    return AddField(
        name, [value](JsonMutator& mutator) { return mutator.SetUint(value); });
  }

  virtual absl::Status AddStringField(absl::string_view name,
                                      absl::string_view value) {
    return AddField(name, [value](JsonMutator& mutator) {
      return mutator.SetString(value);
    });
  }

  virtual absl::Status AddStringField(absl::string_view name,
                                      const absl::Cord& value) {
    return AddField(name, [&value](JsonMutator& mutator) {
      return mutator.SetString(value);
    });
  }

  virtual absl::Status AddBytesField(absl::string_view name,
                                     absl::string_view value) {
    return AddField(name, [value](JsonMutator& mutator) {
      return mutator.SetBytes(value);
    });
  }

  virtual absl::Status AddArrayField(
      absl::string_view name,
      absl::FunctionRef<absl::Status(JsonArrayMutator&)> mutator) {
    return AddField(name, [mutator](JsonMutator& json_mutator) {
      return json_mutator.SetArray(mutator);
    });
  }

  virtual absl::Status AddObjectField(
      absl::string_view name,
      absl::FunctionRef<absl::Status(JsonObjectMutator&)> mutator) {
    return AddField(name, [mutator](JsonMutator& json_mutator) {
      return json_mutator.SetObject(mutator);
    });
  }
};

// `JsonObjectMutator` is an abstract interface for building JSON arrays. It is
// used to facilitate converting values to JSON.
class JsonArrayMutator {
 public:
  virtual ~JsonArrayMutator() = default;

  virtual absl::Status AddElement(
      absl::FunctionRef<absl::Status(JsonMutator&)> mutator) = 0;

  virtual absl::Status AddNullElement() {
    return AddElement([](JsonMutator& mutator) { return mutator.SetNull(); });
  }

  virtual absl::Status AddBoolElement(bool value) {
    return AddElement(
        [value](JsonMutator& mutator) { return mutator.SetBool(value); });
  }

  virtual absl::Status AddNumberElement(double value) {
    return AddElement(
        [value](JsonMutator& mutator) { return mutator.SetNumber(value); });
  }

  virtual absl::Status AddIntElement(int64_t value) {
    return AddElement(
        [value](JsonMutator& mutator) { return mutator.SetInt(value); });
  }

  virtual absl::Status AddUintElement(uint64_t value) {
    return AddElement(
        [value](JsonMutator& mutator) { return mutator.SetUint(value); });
  }

  virtual absl::Status AddStringElement(absl::string_view value) {
    return AddElement(
        [value](JsonMutator& mutator) { return mutator.SetString(value); });
  }

  virtual absl::Status AddStringElement(const absl::Cord& value) {
    return AddElement(
        [&value](JsonMutator& mutator) { return mutator.SetString(value); });
  }

  virtual absl::Status AddBytesElement(absl::string_view value) {
    return AddElement(
        [value](JsonMutator& mutator) { return mutator.SetBytes(value); });
  }

  virtual absl::Status AddArrayElement(
      absl::FunctionRef<absl::Status(JsonArrayMutator&)> mutator) {
    return AddElement([mutator](JsonMutator& json_mutator) {
      return json_mutator.SetArray(mutator);
    });
  }

  virtual absl::Status AddObjectElement(
      absl::FunctionRef<absl::Status(JsonObjectMutator&)> mutator) {
    return AddElement([mutator](JsonMutator& json_mutator) {
      return json_mutator.SetObject(mutator);
    });
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_JSON_H_
