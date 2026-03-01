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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_IPADDRESS_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_IPADDRESS_H_

// copybara:strip_begin(ipaddress)
#include "net/base/ipaddress.h"
/* copybara:strip_end_and_replace
#include "internal/ipaddress_oss.h"
*/

// copybara:strip_begin(ipaddress)
namespace cel::internal {
using ::net_base::IPAddress;
using ::net_base::IPRange;

using ::net_base::GetMappedIPv4Address;
using ::net_base::HostUInt32ToIPAddress;
using ::net_base::IPAddressToHostUInt32;
using ::net_base::IsAnyIPAddress;
using ::net_base::IsLoopbackIPAddress;
using ::net_base::IsProperSubRange;
using ::net_base::IsV4MulticastIPAddress;
using ::net_base::PackedStringToIPAddress;
using ::net_base::StringToIPAddress;
using ::net_base::StringToIPRangeAndTruncate;
}  // namespace cel::internal
// copybara:strip_end

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_IPADDRESS_H_
