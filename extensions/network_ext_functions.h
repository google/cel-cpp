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
// extensions/network_ext_functions.h

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_NETWORK_EXT_FUNCTIONS_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_NETWORK_EXT_FUNCTIONS_H_

#include <string>

#include "net/base/ipaddress.h"
#include "common/native_type.h"
#include "common/typeinfo.h"
#include "common/value.h"
#include "runtime/function.h"
#include "google/protobuf/arena.h"

namespace cel::extensions {

// ... IpAddrRep and CidrRangeRep classes ...
class IpAddrRep {
 public:
  static cel::Value Create(google::protobuf::Arena* arena,
                           const net_base::IPAddress& addr);
  static const IpAddrRep* Unwrap(const cel::Value& value);
  IpAddrRep() = default;
  explicit IpAddrRep(const net_base::IPAddress& addr) : addr_(addr) {}
  const net_base::IPAddress& addr() const { return addr_; }
  bool Equals(const IpAddrRep& other) const { return addr_ == other.addr_; }
  std::string DebugString() const;
  static cel::NativeTypeId GetTypeId() { return cel::TypeId<IpAddrRep>(); }

 private:
  net_base::IPAddress addr_;
};

class CidrRangeRep {
 public:
  static cel::Value Create(google::protobuf::Arena* arena,
                           const net_base::IPAddress& host, int length);
  static const CidrRangeRep* Unwrap(const cel::Value& value);

  CidrRangeRep() = default;
  explicit CidrRangeRep(const net_base::IPAddress& host, int length)
      : host_(host), length_(length) {}

  const net_base::IPAddress& host() const { return host_; }
  int length() const { return length_; }

  // Utility to get the net_base::IPRange (which will be truncated)
  net_base::IPRange ToIPRange() const {
    return net_base::IPRange(host_, length_);
  }

  bool Equals(const CidrRangeRep& other) const {
    return length_ == other.length_ && host_ == other.host_;
  }
  std::string DebugString() const;

  static cel::NativeTypeId GetTypeId() { return cel::TypeId<CidrRangeRep>(); }

  template <typename H>
  friend H AbslHashValue(H h, const CidrRangeRep& c) {
    return H::combine(std::move(h), c.host_, c.length_);
  }

 private:
  net_base::IPAddress host_;
  int length_ = -1;
};

// Declarations
cel::Value NetIsIP(const cel::StringValue& str_val,
                   const cel::Function::InvokeContext& context);
cel::Value NetIPString(const cel::StringValue& str_val,
                       const cel::Function::InvokeContext& context);
cel::Value NetIsCIDR(const cel::StringValue& str_val,
                     const cel::Function::InvokeContext& context);
cel::Value NetCIDRString(const cel::StringValue& str_val,
                         const cel::Function::InvokeContext& context);
cel::Value NetIPFamily(const cel::OpaqueValue& self,
                       const cel::Function::InvokeContext& context);
cel::Value NetIPIsLoopback(const cel::OpaqueValue& self,
                           const cel::Function::InvokeContext& context);
cel::Value NetIPIsGlobalUnicast(const cel::OpaqueValue& self,
                                const cel::Function::InvokeContext& context);
cel::Value NetIPIsLinkLocalMulticast(
    const cel::OpaqueValue& self, const cel::Function::InvokeContext& context);
cel::Value NetIPIsLinkLocalUnicast(const cel::OpaqueValue& self,
                                   const cel::Function::InvokeContext& context);
cel::Value NetIPIsUnspecified(const cel::OpaqueValue& self,
                              const cel::Function::InvokeContext& context);
cel::Value NetIPIsCanonical(const cel::StringValue& str_val,
                            const cel::Function::InvokeContext& context);
cel::Value NetCIDRContainsIP(const cel::OpaqueValue& self,
                             const cel::OpaqueValue& other,
                             const cel::Function::InvokeContext& context);
cel::Value NetCIDRContainsIPString(const cel::OpaqueValue& self,
                                   const cel::StringValue& other_str,
                                   const cel::Function::InvokeContext& context);
cel::Value NetCIDRContainsCIDR(const cel::OpaqueValue& self,
                               const cel::OpaqueValue& other,
                               const cel::Function::InvokeContext& context);
cel::Value NetCIDRContainsCIDRString(
    const cel::OpaqueValue& self, const cel::StringValue& other_str,
    const cel::Function::InvokeContext& context);
cel::Value NetCIDRIP(const cel::OpaqueValue& self,
                     const cel::Function::InvokeContext& context);
cel::Value NetCIDRMasked(const cel::OpaqueValue& self,
                         const cel::Function::InvokeContext& context);
cel::Value NetCIDRPrefixLength(const cel::OpaqueValue& self,
                               const cel::Function::InvokeContext& context);
cel::Value NetToString(const cel::OpaqueValue& self,
                       const cel::Function::InvokeContext& context);

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_NETWORK_EXT_FUNCTIONS_H_
