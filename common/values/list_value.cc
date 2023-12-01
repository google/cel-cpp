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

#include <cstddef>
#include <memory>

#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/value.h"
#include "internal/status_macros.h"

namespace cel {

class ListValueInterfaceIterator final : public ListValueIterator {
 public:
  explicit ListValueInterfaceIterator(const ListValueInterface& interface)
      : interface_(interface), size_(interface_.Size()) {}

  bool HasNext() override { return index_ < size_; }

  absl::StatusOr<ValueView> Next() override {
    if (ABSL_PREDICT_FALSE(index_ >= size_)) {
      return absl::FailedPreconditionError(
          "ListValueIterator::Next() called when "
          "ListValueIterator::HasNext() returns false");
    }
    return interface_.GetImpl(index_++);
  }

 private:
  const ListValueInterface& interface_;
  const size_t size_;
  size_t index_ = 0;
};

absl::StatusOr<ValueView> ListValueInterface::Get(size_t index) const {
  if (ABSL_PREDICT_FALSE(index >= Size())) {
    return absl::InvalidArgumentError("index out of bounds");
  }
  return GetImpl(index);
}

absl::Status ListValueInterface::ForEach(ForEachCallback callback) const {
  const size_t size = Size();
  for (size_t index = 0; index < size; ++index) {
    CEL_ASSIGN_OR_RETURN(auto element, GetImpl(index));
    CEL_ASSIGN_OR_RETURN(auto ok, callback(element));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<absl::Nonnull<ListValueIteratorPtr>>
ListValueInterface::NewIterator() const {
  return std::make_unique<ListValueInterfaceIterator>(*this);
}

}  // namespace cel
