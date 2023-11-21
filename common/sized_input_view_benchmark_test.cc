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

#include <cstdint>
#include <list>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "common/sized_input_view.h"
#include "internal/benchmark.h"
#include "internal/testing.h"

namespace cel {
namespace {

// Baseline. Iterates over a container.
template <typename C>
void BM_Iter(::benchmark::State& state) {
  C c(1 << 10);

  while (state.KeepRunningBatch(c.size())) {
    benchmark::DoNotOptimize(&c);
    for (const auto& e : c) {
      benchmark::DoNotOptimize(&e);
    }
  }
}

// Same as BM_Iter, but uses SizedInputView to iterate.
template <typename C>
void BM_ViewIter(::benchmark::State& state) {
  C c(1 << 10);
  SizedInputView<typename C::value_type> v(c);

  while (state.KeepRunningBatch(c.size())) {
    benchmark::DoNotOptimize(v);
    for (auto& e : v) {
      benchmark::DoNotOptimize(e);
    }
  }
}

BENCHMARK_TEMPLATE(BM_Iter, std::list<int64_t>);
BENCHMARK_TEMPLATE(BM_ViewIter, std::list<int64_t>);

BENCHMARK_TEMPLATE(BM_Iter, std::vector<int64_t>);
BENCHMARK_TEMPLATE(BM_ViewIter, std::vector<int64_t>);

BENCHMARK_TEMPLATE(BM_Iter, std::list<std::string>);
BENCHMARK_TEMPLATE(BM_ViewIter, std::list<std::string>);

BENCHMARK_TEMPLATE(BM_Iter, std::vector<std::string>);
BENCHMARK_TEMPLATE(BM_ViewIter, std::vector<std::string>);

// Same as BM_ViewIter, but converts strings to string_views.
void BM_StringViewIter(::benchmark::State& state) {
  std::vector<std::string> c(1 << 10);
  SizedInputView<absl::string_view> v(c);

  while (state.KeepRunningBatch(c.size())) {
    benchmark::DoNotOptimize(v);
    for (auto& e : v) {
      benchmark::DoNotOptimize(e);
    }
  }
}

BENCHMARK(BM_StringViewIter);

}  // namespace
}  // namespace cel
