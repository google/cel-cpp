// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_IS_MESSAGE_LITE_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_IS_MESSAGE_LITE_H_

#include <type_traits>

#include "google/protobuf/message.h"

namespace cel::extensions::protobuf_internal {

template <typename T>
inline constexpr bool NotMessageLite = std::is_base_of_v<google::protobuf::Message, T>;

template <typename T>
inline constexpr bool IsMessageLite = !NotMessageLite<T>;

}  // namespace cel::extensions::protobuf_internal

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_IS_MESSAGE_LITE_H_
