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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_IPADDRESS_UTIL_OSS_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_IPADDRESS_UTIL_OSS_H_

namespace cel::internal {

class IPAddress;

// Returns true if ip is :: or 0.0.0.0.
bool IsAnyIPAddress(const IPAddress& ip);

// Returns true if ip is in 127.0.0.0/8 or is ::1.
bool IsLoopbackIPAddress(const IPAddress& ip);

// Returns true if ip is in 169.254.0.0/16 or fe80::/10.
bool IsLinkLocalIP(const IPAddress& ip);

// Returns true if ip is in 224.0.0.0/4.
bool IsV4MulticastIPAddress(const IPAddress& ip);

// Returns true if ip is a private address (RFC1918) or IPv6 ULA (fc00::/7).
bool IsPrivateIP(const IPAddress& ip);

// Returns true if IP is not globally routable.
bool IsNonRoutableIP(const IPAddress& ip);

// If ip6 is an IPv4-mapped IPv6 address, stores the IPv4 address in *ip4 and
// returns true. Otherwise returns false.
bool GetMappedIPv4Address(const IPAddress& ip6, IPAddress* ip4);

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_IPADDRESS_UTIL_OSS_H_
