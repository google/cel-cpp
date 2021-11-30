/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_CONCAT_LIST_IMPL_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_CONCAT_LIST_IMPL_H_

#include <vector>

#include "eval/public/cel_value.h"

namespace google::api::expr::runtime {

// Mutable CelList implementation intended to be used in the accumulation of
// a list within a comprehension loop.
//
// This value should only ever be used as an intermediate result from CEL and
// not within user code.
class MutableListImpl : public CelList {
 public:
  // Create a list from an initial vector of CelValues.
  explicit MutableListImpl(std::vector<CelValue> values)
      : values_(std::move(values)) {}

  // List size.
  int size() const override { return values_.size(); }

  // Append a single element to the list.
  void Append(const CelValue& element) { values_.push_back(element); }

  // List element access operator.
  CelValue operator[](int index) const override { return values_[index]; }

 private:
  std::vector<CelValue> values_;
};

}  //  namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_CONCAT_LIST_IMPL_H_
