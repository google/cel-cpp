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

#include "internal/ipaddress_oss.h"
#include "internal/testing.h"

namespace cel::internal {
namespace {

TEST(IPAddressTest, IsAnyIPAddress) {
  EXPECT_TRUE(IsAnyIPAddress(IPAddress::Any4()));
  EXPECT_TRUE(IsAnyIPAddress(IPAddress::Any6()));
  EXPECT_FALSE(IsAnyIPAddress(StringToIPAddressOrDie("1.2.3.4")));
  EXPECT_FALSE(IsAnyIPAddress(StringToIPAddressOrDie("::1")));
}

TEST(IPAddressTest, IsLoopbackIPAddress) {
  EXPECT_TRUE(IsLoopbackIPAddress(StringToIPAddressOrDie("127.0.0.1")));
  EXPECT_TRUE(IsLoopbackIPAddress(StringToIPAddressOrDie("127.1.2.3")));
  EXPECT_FALSE(IsLoopbackIPAddress(StringToIPAddressOrDie("128.0.0.1")));
  EXPECT_TRUE(IsLoopbackIPAddress(IPAddress::Loopback6()));
  EXPECT_FALSE(IsLoopbackIPAddress(StringToIPAddressOrDie("::2")));
}

TEST(IPAddressTest, IsLinkLocalIP) {
  EXPECT_TRUE(IsLinkLocalIP(StringToIPAddressOrDie("169.254.0.1")));
  EXPECT_TRUE(IsLinkLocalIP(StringToIPAddressOrDie("169.254.100.200")));
  EXPECT_FALSE(IsLinkLocalIP(StringToIPAddressOrDie("169.253.0.1")));
  EXPECT_TRUE(IsLinkLocalIP(StringToIPAddressOrDie("fe80::1")));
  EXPECT_FALSE(IsLinkLocalIP(StringToIPAddressOrDie("fec0::1")));
}

TEST(IPAddressTest, GetMappedIPv4Address) {
  IPAddress ipv4_mapped_ipv6 = StringToIPAddressOrDie("::ffff:192.168.0.1");
  IPAddress ipv4;
  EXPECT_TRUE(GetMappedIPv4Address(ipv4_mapped_ipv6, &ipv4));
  EXPECT_EQ(ipv4, StringToIPAddressOrDie("192.168.0.1"));

  IPAddress ipv6 = StringToIPAddressOrDie("::1");
  EXPECT_FALSE(GetMappedIPv4Address(ipv6, &ipv4));
}

TEST(IPAddressTest, IsV4MulticastIPAddress) {
  EXPECT_TRUE(IsV4MulticastIPAddress(StringToIPAddressOrDie("224.0.0.1")));
  EXPECT_TRUE(
      IsV4MulticastIPAddress(StringToIPAddressOrDie("239.255.255.255")));
  EXPECT_FALSE(
      IsV4MulticastIPAddress(StringToIPAddressOrDie("223.255.255.255")));
  EXPECT_FALSE(IsV4MulticastIPAddress(StringToIPAddressOrDie("::1")));
}

TEST(IPAddressTest, IsPrivateIP) {
  EXPECT_TRUE(IsPrivateIP(StringToIPAddressOrDie("10.0.0.1")));
  EXPECT_TRUE(IsPrivateIP(StringToIPAddressOrDie("172.16.0.1")));
  EXPECT_TRUE(IsPrivateIP(StringToIPAddressOrDie("172.31.255.254")));
  EXPECT_TRUE(IsPrivateIP(StringToIPAddressOrDie("192.168.0.1")));
  EXPECT_FALSE(IsPrivateIP(StringToIPAddressOrDie("11.0.0.1")));
  EXPECT_FALSE(IsPrivateIP(StringToIPAddressOrDie("172.15.0.1")));
  EXPECT_FALSE(IsPrivateIP(StringToIPAddressOrDie("172.32.0.1")));
  EXPECT_FALSE(IsPrivateIP(StringToIPAddressOrDie("192.169.0.1")));
  EXPECT_TRUE(IsPrivateIP(StringToIPAddressOrDie("fc00::1")));
  EXPECT_FALSE(IsPrivateIP(StringToIPAddressOrDie("fe00::1")));
}

TEST(IPAddressTest, IsNonRoutableIP) {
  EXPECT_TRUE(IsNonRoutableIP(StringToIPAddressOrDie("10.0.0.1")));
  EXPECT_TRUE(IsNonRoutableIP(StringToIPAddressOrDie("127.0.0.1")));
  EXPECT_TRUE(IsNonRoutableIP(StringToIPAddressOrDie("169.254.0.1")));
  EXPECT_TRUE(IsNonRoutableIP(StringToIPAddressOrDie("0.0.0.0")));
  EXPECT_TRUE(IsNonRoutableIP(StringToIPAddressOrDie("224.0.0.1")));
  EXPECT_FALSE(IsNonRoutableIP(StringToIPAddressOrDie("8.8.8.8")));
  EXPECT_TRUE(IsNonRoutableIP(StringToIPAddressOrDie("fc00::1")));
  // Current implementation for IPv6 only checks IsPrivateIP.
  EXPECT_FALSE(IsNonRoutableIP(StringToIPAddressOrDie("::1")));
  EXPECT_FALSE(IsNonRoutableIP(StringToIPAddressOrDie("fe80::1")));
}

}  // namespace
}  // namespace cel::internal
