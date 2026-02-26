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

#include "extensions/network_ext_functions.h"

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "common/ipaddress_oss.h"
#include "common/value.h"
#include "common/values/error_value.h"
#include "runtime/function.h"

namespace cel::extensions {

namespace {

using ::cel::BoolValue;
using ::cel::ErrorValue;
using ::cel::Function;
using ::cel::GetMappedIPv4Address;
using ::cel::IntValue;
using ::cel::IPAddress;
using ::cel::IPRange;
using ::cel::IsAnyIPAddress;
using ::cel::IsInitializedAddress;
using ::cel::IsLinkLocalIP;
using ::cel::IsLoopbackIPAddress;
using ::cel::IsNonRoutableIP;
using ::cel::IsProperSubRange;
using ::cel::IsWithinSubnet;
using ::cel::StringToIPAddress;
using ::cel::StringValue;
using ::cel::Value;

// -----------------------------------------------------------------------------
// Strict Parsing Helpers
// -----------------------------------------------------------------------------

bool IsStrictIP(const IPAddress& addr) {
  if (!IsInitializedAddress(addr)) return false;
  IPAddress unused;
  // Check for IPv4-mapped IPv6 addresses.
  if (GetMappedIPv4Address(addr, &unused)) {
    return false;
  }
  // zone() is not a member of net_base::IPAddress
  return true;
}

// Helper to parse CIDR string into host and length without truncation
bool ParseCIDRWithoutTruncation(absl::string_view s, IPAddress* host,
                                int* length) {
  std::vector<absl::string_view> parts = absl::StrSplit(s, '/');
  if (parts.size() != 2) {
    return false;
  }
  if (!StringToIPAddress(parts[0], host)) {
    return false;
  }
  if (!absl::SimpleAtoi(parts[1], length)) {
    return false;
  }
  if ((*length < 0 || (*host).is_ipv4()) &&
      (*length > 32 || (*host).is_ipv6()) && *length > 128) {
    return false;
  }
  return true;
}

bool IsStrictCIDR(const IPAddress& host, int length) {
  if (!IsInitializedAddress(host)) return false;
  return IsStrictIP(host);
}

}  // namespace

// -----------------------------------------------------------------------------
// CEL Function Implementations
// -----------------------------------------------------------------------------

// isIP(string) -> bool
Value NetIsIP(const StringValue& str_val,
              const Function::InvokeContext& context) {
  IPAddress addr;
  if (!StringToIPAddress(std::string(str_val.ToString()), &addr)) {
    return BoolValue(false);
  }
  return BoolValue(IsStrictIP(addr));
}

// ip(string) -> net.IP
Value NetIPString(const StringValue& str_val,
                  const Function::InvokeContext& context) {
  std::string str = std::string(str_val.ToString());
  IPAddress addr;
  if (!StringToIPAddress(str, &addr)) {
    return ErrorValue(absl::InvalidArgumentError(
        absl::StrCat("IP Address '", str, "' parse error")));
  }
  if (!IsStrictIP(addr)) {
    return ErrorValue(absl::InvalidArgumentError(absl::StrCat(
        "IP Address '", str, "' is not a strict IP (e.g., mapped IPv4)")));
  }
  return IpAddrRep::Create(context.arena(), addr);
}

// isCIDR(string) -> bool
Value NetIsCIDR(const StringValue& str_val,
                const Function::InvokeContext& context) {
  std::string str = std::string(str_val.ToString());
  IPAddress host;
  int length;
  if (!ParseCIDRWithoutTruncation(str, &host, &length)) {
    return BoolValue(false);
  }
  return BoolValue(IsStrictCIDR(host, length));
}

// cidr(string) -> net.CIDR
Value NetCIDRString(const StringValue& str_val,
                    const Function::InvokeContext& context) {
  std::string str = std::string(str_val.ToString());
  IPAddress host;
  int length;
  if (!ParseCIDRWithoutTruncation(str, &host, &length)) {
    return ErrorValue(absl::InvalidArgumentError(
        absl::StrCat("CIDR '", str, "' parse error")));
  }

  if (!IsStrictCIDR(host, length)) {
    return ErrorValue(absl::InvalidArgumentError(absl::StrCat(
        "CIDR '", str, "' is not a strict CIDR (e.g., mapped IPv4)")));
  }
  return CidrRangeRep::Create(context.arena(), host, length);
}

// <ip>.family() -> int
Value NetIPFamily(const OpaqueValue& self,
                  const Function::InvokeContext& context) {
  const IpAddrRep* rep = IpAddrRep::Unwrap(self);
  if (!rep || !IsInitializedAddress(rep->addr())) {
    return ErrorValue(absl::InvalidArgumentError("Uninitialized IPAddress"));
  }
  switch (rep->addr().address_family()) {
    case AF_INET:
      return IntValue(4);
    case AF_INET6:
      return IntValue(6);
    default:
      return ErrorValue(absl::InvalidArgumentError("Unknown address family"));
  }
}

// <ip>.isLoopback() -> bool
Value NetIPIsLoopback(const OpaqueValue& self,
                      const Function::InvokeContext& context) {
  const IpAddrRep* rep = IpAddrRep::Unwrap(self);
  if (!rep || !IsInitializedAddress(rep->addr())) {
    return ErrorValue(absl::InvalidArgumentError("Uninitialized IPAddress"));
  }
  return BoolValue(IsLoopbackIPAddress(rep->addr()));
}

// <ip>.isGlobalUnicast() -> bool
Value NetIPIsGlobalUnicast(const OpaqueValue& self,
                           const Function::InvokeContext& context) {
  const IpAddrRep* rep = IpAddrRep::Unwrap(self);
  if (!rep || !IsInitializedAddress(rep->addr())) {
    return ErrorValue(absl::InvalidArgumentError("Uninitialized IPAddress"));
  }
  const IPAddress& addr = rep->addr();

  if (IsAnyIPAddress(addr) || addr == IPAddress::Loopback4() ||
      addr == IPAddress::Loopback6() || IsLinkLocalIP(addr) ||
      IsNonRoutableIP(addr)) {
    return BoolValue(false);
  }

  if (addr.is_ipv4()) {
    return BoolValue(!IsV4MulticastIPAddress(addr));
  }

  if (addr.is_ipv6()) {
    in6_addr addr6 = addr.ipv6_address();
    if (IN6_IS_ADDR_MULTICAST(&addr6)) {
      return BoolValue(false);
    }
    return BoolValue(true);
  }
  return BoolValue(false);
}

Value NetIPIsLinkLocalMulticast(const OpaqueValue& self,
                                const Function::InvokeContext& context) {
  const IpAddrRep* rep = IpAddrRep::Unwrap(self);
  if (!rep || !rep->addr().is_ipv6()) {
    return BoolValue(false);
  }
  in6_addr addr6 = rep->addr().ipv6_address();
  return BoolValue(IN6_IS_ADDR_MC_LINKLOCAL(&addr6));
}

Value NetIPIsLinkLocalUnicast(const OpaqueValue& self,
                              const Function::InvokeContext& context) {
  const IpAddrRep* rep = IpAddrRep::Unwrap(self);
  if (!rep || !IsInitializedAddress(rep->addr())) {
    return ErrorValue(absl::InvalidArgumentError("Uninitialized IPAddress"));
  }
  return BoolValue(IsLinkLocalIP(rep->addr()));
}

Value NetIPIsUnspecified(const OpaqueValue& self,
                         const Function::InvokeContext& context) {
  const IpAddrRep* rep = IpAddrRep::Unwrap(self);
  if (!rep) {
    return ErrorValue(absl::InvalidArgumentError("Invalid IP object"));
  }
  return BoolValue(IsAnyIPAddress(rep->addr()));
}

Value NetIPIsCanonical(const StringValue& str_val,
                       const Function::InvokeContext& context) {
  std::string str = std::string(str_val.ToString());
  IPAddress addr;
  if (!StringToIPAddress(str, &addr)) {
    return BoolValue(false);
  }
  if (!IsStrictIP(addr)) {
    return BoolValue(false);
  }
  return BoolValue(addr.ToString() == str);
}

Value NetCIDRContainsIP(const OpaqueValue& self, const OpaqueValue& other,
                        const Function::InvokeContext& context) {
  const CidrRangeRep* self_rep = CidrRangeRep::Unwrap(self);
  const IpAddrRep* other_rep = IpAddrRep::Unwrap(other);
  if (!self_rep || !IsInitializedAddress(self_rep->host()) ||
      self_rep->length() < 0 || !other_rep ||
      !IsInitializedAddress(other_rep->addr())) {
    return ErrorValue(absl::InvalidArgumentError("Uninitialized CIDR or IP"));
  }
  return BoolValue(IsWithinSubnet(self_rep->ToIPRange(), other_rep->addr()));
}

Value NetCIDRContainsIPString(const OpaqueValue& self,
                              const StringValue& other_str,
                              const Function::InvokeContext& context) {
  const CidrRangeRep* self_rep = CidrRangeRep::Unwrap(self);
  if (!self_rep || !IsInitializedAddress(self_rep->host()) ||
      self_rep->length() < 0) {
    return ErrorValue(absl::InvalidArgumentError("Uninitialized CIDR"));
  }

  std::string str = std::string(other_str.ToString());
  IPAddress other_addr;
  if (!StringToIPAddress(str, &other_addr) || !IsStrictIP(other_addr)) {
    return ErrorValue(absl::InvalidArgumentError(
        absl::StrCat("Invalid or non-strict IP string: ", str)));
  }
  return BoolValue(IsWithinSubnet(self_rep->ToIPRange(), other_addr));
}

Value NetCIDRContainsCIDR(const OpaqueValue& self, const OpaqueValue& other,
                          const Function::InvokeContext& context) {
  const CidrRangeRep* self_rep = CidrRangeRep::Unwrap(self);
  const CidrRangeRep* other_rep = CidrRangeRep::Unwrap(other);
  if (!self_rep || !IsInitializedAddress(self_rep->host()) ||
      self_rep->length() < 0 || !other_rep ||
      !IsInitializedAddress(other_rep->host()) || other_rep->length() < 0) {
    return ErrorValue(absl::InvalidArgumentError("Uninitialized CIDR"));
  }
  IPRange self_range = self_rep->ToIPRange();
  IPRange other_range = other_rep->ToIPRange();
  return BoolValue(self_range == other_range ||
                   IsProperSubRange(self_range, other_range));
}

Value NetCIDRContainsCIDRString(const OpaqueValue& self,
                                const StringValue& other_str,
                                const Function::InvokeContext& context) {
  const CidrRangeRep* self_rep = CidrRangeRep::Unwrap(self);
  if (!self_rep || !IsInitializedAddress(self_rep->host()) ||
      self_rep->length() < 0) {
    return ErrorValue(absl::InvalidArgumentError("Uninitialized CIDR"));
  }

  std::string str = std::string(other_str.ToString());
  IPAddress other_host;
  int other_length;
  if (!ParseCIDRWithoutTruncation(str, &other_host, &other_length) ||
      !IsStrictCIDR(other_host, other_length)) {
    return ErrorValue(absl::InvalidArgumentError(
        absl::StrCat("Invalid or non-strict CIDR string: ", str)));
  }
  IPRange self_range = self_rep->ToIPRange();
  IPRange other_range(other_host, other_length);
  return BoolValue(self_range == other_range ||
                   IsProperSubRange(self_range, other_range));
}

Value NetCIDRIP(const OpaqueValue& self,
                const Function::InvokeContext& context) {
  const CidrRangeRep* rep = CidrRangeRep::Unwrap(self);
  if (!rep || !IsInitializedAddress(rep->host()) || rep->length() < 0) {
    return ErrorValue(absl::InvalidArgumentError("Uninitialized CIDR"));
  }
  return IpAddrRep::Create(context.arena(), rep->host());
}

Value NetCIDRMasked(const OpaqueValue& self,
                    const Function::InvokeContext& context) {
  const CidrRangeRep* rep = CidrRangeRep::Unwrap(self);
  if (!rep || !IsInitializedAddress(rep->host()) || rep->length() < 0) {
    return ErrorValue(absl::InvalidArgumentError("Uninitialized CIDR"));
  }
  IPRange masked_range = rep->ToIPRange();
  return CidrRangeRep::Create(context.arena(), masked_range.host(),
                              masked_range.length());
}

Value NetCIDRPrefixLength(const OpaqueValue& self,
                          const Function::InvokeContext& context) {
  const CidrRangeRep* rep = CidrRangeRep::Unwrap(self);
  if (!rep || !IsInitializedAddress(rep->host()) || rep->length() < 0) {
    return ErrorValue(absl::InvalidArgumentError("Uninitialized CIDR"));
  }
  return IntValue(rep->length());
}

Value NetToString(const OpaqueValue& self,
                  const Function::InvokeContext& context) {
  if (const IpAddrRep* rep = IpAddrRep::Unwrap(self)) {
    if (!IsInitializedAddress(rep->addr())) {
      return ErrorValue(absl::InvalidArgumentError("Uninitialized IPAddress"));
    }
    return StringValue::From(rep->addr().ToString(), context.arena());
  }
  if (const CidrRangeRep* rep = CidrRangeRep::Unwrap(self)) {
    if (!IsInitializedAddress(rep->host()) || rep->length() < 0) {
      return ErrorValue(absl::InvalidArgumentError("Uninitialized CIDR"));
    }
    return StringValue::From(
        absl::StrCat(rep->host().ToString(), "/", rep->length()),
        context.arena());
  }
  return ErrorValue(
      absl::InvalidArgumentError("Unsupported type for string()"));
}

}  // namespace cel::extensions
