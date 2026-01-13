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

#include "internal/ipaddress_util_oss.h"

#include <cstdint>

#include "absl/log/absl_check.h"
#include "internal/ipaddress_oss.h"

#define UNALIGNED_LOAD32(p) \
  cel::internal::ipaddress_internal::UnalignedLoad<uint32_t>(p)
#define UNALIGNED_LOAD64(p) \
  cel::internal::ipaddress_internal::UnalignedLoad<uint64_t>(p)

namespace cel::internal {

bool IsAnyIPAddress(const IPAddress& ip) {
  switch (ip.address_family()) {
    case AF_INET:
      return ip.ipv4_address().s_addr == INADDR_ANY;
    case AF_INET6:
      return ip == IPAddress(in6addr_any);
    default:
      return false;
  }
}

bool IsLoopbackIPAddress(const IPAddress& ip) {
  switch (ip.address_family()) {
    case AF_INET:
      return (IPAddressToHostUInt32(ip) & 0xff000000U) == 0x7f000000U;
    case AF_INET6:
      return ip == IPAddress::Loopback6();
    default:
      return false;
  }
}

bool IsLinkLocalIP(const IPAddress& ip) {
  if (ip.is_ipv4()) {
    // 169.254.0.0/16
    return (IPAddressToHostUInt32(ip) & 0xffff0000U) == 0xa9fe0000U;
  }
  if (ip.is_ipv6()) {
    // Store the address in a variable (address of temporary object)
    const in6_addr addr6 = ip.ipv6_address();
    return IN6_IS_ADDR_LINKLOCAL(&addr6);
  }
  return false;
}

bool IsV4MulticastIPAddress(const IPAddress& ip) {
  if (!ip.is_ipv4()) {
    return false;
  }
  return (IPAddressToHostUInt32(ip) & 0xf0000000U) == 0xe0000000U;
}

namespace {
bool IsUniqueLocalIP(const IPAddress& ip) {
  return ip.is_ipv6() && (ip.ipv6_address().s6_addr[0] & 0xfe) == 0xfc;
}
}  // namespace

bool IsPrivateIP(const IPAddress& ip) {
  if (ip.is_ipv4()) {
    uint32_t h = IPAddressToHostUInt32(ip);
    return (h & 0xff000000U) == 0x0a000000U ||  // 10.0.0.0/8
           (h & 0xfff00000U) == 0xac100000U ||  // 172.16.0.0/12
           (h & 0xffff0000U) == 0xc0a80000U;    // 192.168.0.0/16
  } else if (ip.is_ipv6()) {
    return IsUniqueLocalIP(ip);
  }
  return false;
}

bool IsNonRoutableIP(const IPAddress& ip) {
  return !ip.is_ipv4()
             ? IsPrivateIP(ip)
             : IsPrivateIP(ip) || IsLinkLocalIP(ip) ||
                   IsLoopbackIPAddress(ip) ||
                   // 0.0.0.0/8
                   (IPAddressToHostUInt32(ip) & 0xff000000U) == 0x00000000U ||
                   // 224.0.0.0/3
                   (IPAddressToHostUInt32(ip) & 0xe0000000U) == 0xe0000000U;
}

bool GetMappedIPv4Address(const IPAddress& ip6, IPAddress* ip4) {
  if (ip6.address_family() != AF_INET6) {
    ABSL_DCHECK_NE(AF_UNSPEC, ip6.address_family());
    return false;
  }

  in6_addr addr6 = ip6.ipv6_address();
  if (UNALIGNED_LOAD64(addr6.s6_addr16) != 0 || addr6.s6_addr16[4] != 0 ||
      addr6.s6_addr16[5] != 0xffff) {
    return false;
  }

  if (ip4) {
    in_addr ipv4;
    ipv4.s_addr = UNALIGNED_LOAD32(addr6.s6_addr16 + 6);
    *ip4 = IPAddress(ipv4);
  }

  return true;
}

}  // namespace cel::internal
