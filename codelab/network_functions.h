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
//
// Example extension library for introducing an OpaqueValue type.
//
// The address handling is simplified for the example, and IPv6 is
// unimplemented. Do not use this as-is.

#ifndef THIRD_PARTY_CEL_CPP_CODELAB_NETWORK_FUNCTIONS_H_
#define THIRD_PARTY_CEL_CPP_CODELAB_NETWORK_FUNCTIONS_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/value.h"
#include "compiler/compiler.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"
#include "runtime/type_registry.h"
#include "google/protobuf/arena.h"

namespace cel_codelab {

enum class IpVersion : uint8_t {
  kUnset = 0,
  kIPv4 = 4,
  kIPv6 = 6,  // unimplemented, but present for illustration.
};

// Represents a network address. To simplify the CEL type representation, this
// only supports IPv4.
//
// A the default value of 0v0 is special, and represents an invalid address,
// comparing unequal to anything except itself. For the purposes of ordering,
// compares less than any valid address.
//
// The example extension functions include a version that returns a zero value
// on error and a version that returns a CEL error.
//
// This class is stored inline in the OpaqueValue because it is compact and
// trivially copyable.
class NetworkAddressRep {
 public:
  // Creates a Value that wraps the given NetworkAddress. The representation is
  // copied to the provided arena.
  static cel::Value MakeValue(const NetworkAddressRep& rep);

  // Unwraps a Value into a NetworkAddressRep. Returns nullptr if the value is
  // not a NetworkAddress.
  static absl::optional<NetworkAddressRep> Unwrap(const cel::Value& value);

  // Parses a string representation of a network address. Returns nullopt if
  // the string is not a valid network address.
  //
  // TODO(uncreated-issue/86): error handling simplified for example, real usage should
  // provide some diagnostic for the parse failure.
  static absl::optional<NetworkAddressRep> Parse(absl::string_view str);

  // Zero value for an invalid address.
  NetworkAddressRep() : addr_({0}), version_(IpVersion::kUnset) {}
  NetworkAddressRep(const NetworkAddressRep& other) = default;
  NetworkAddressRep(NetworkAddressRep&& other) = default;
  NetworkAddressRep& operator=(const NetworkAddressRep& other) = default;
  NetworkAddressRep& operator=(NetworkAddressRep&& other) = default;

  IpVersion version() const { return version_; }

  bool IsZeroValue() const { return version_ == IpVersion::kUnset; }
  bool IsIPv4() const { return version_ == IpVersion::kIPv4; }
  bool IsIPv6() const { return false; }

  absl::optional<uint32_t> TryGetIPv4() const {
    if (version_ == IpVersion::kIPv4) {
      return addr_.v4;
    }
    return absl::nullopt;
  }

  absl::string_view TryGetIPv6() const { return absl::string_view(); }

  std::string Format() const {
    if (version_ == IpVersion::kUnset) {
      return "null";
    }
    if (version_ == IpVersion::kIPv4) {
      return absl::StrCat(
          (addr_.v4 & 0xFF000000) >> 24, ".", (addr_.v4 & 0x00FF0000) >> 16,
          ".", (addr_.v4 & 0x0000FF00) >> 8, ".", (addr_.v4 & 0x000000FF));
    }
    return "v6 not yet implemented";
  }

  uint32_t GetIPv4() const { return addr_.v4; }

  bool IsEqualTo(const NetworkAddressRep& other) const;
  bool IsLessThan(const NetworkAddressRep& other) const;

 private:
  union {
    uint32_t v0;  // zero value
    // Integer representation of an IPv4 address (system byte order)
    uint32_t v4;
    // TO_DO : add ipv6. this prevents storing the value inline due to size, so
    // skipped here.
  } addr_;
  IpVersion version_;
};

// Represents a matcher for network addresses.
//
// Simple implementation that just stores a list of matching ranges.
//
// This is too big to store inline and has non-trivial copy and move behavior,
// so the inline representation is a pointer to an arena-allocated object.
class NetworkAddressMatcher {
 public:
  // Creates a Value that wraps the given NetworkAddress.
  static cel::Value MakeValue(google::protobuf::Arena* arena, NetworkAddressMatcher rep);

  // Unwraps a Value into a NetworkAddressMatcher. Returns nullptr if the value
  // is not a NetworkAddressMatcher.
  static const NetworkAddressMatcher* Unwrap(const cel::Value& value);

  // Parses a string representation of a network address matcher. Returns
  // nullopt if the string is not a valid network address matcher.
  //
  // TODO(uncreated-issue/86): supports a simple IPv4 range for illustration: e.g.
  // 8.8.0.0-8.8.255.255
  static absl::optional<NetworkAddressMatcher> Parse(absl::string_view str);

  // Default value for an empty matcher. Matches nothing.
  NetworkAddressMatcher() = default;
  NetworkAddressMatcher(const NetworkAddressMatcher& other) = default;
  NetworkAddressMatcher(NetworkAddressMatcher&& other) = default;
  NetworkAddressMatcher& operator=(const NetworkAddressMatcher& other) =
      default;
  NetworkAddressMatcher& operator=(NetworkAddressMatcher&& other) = default;

  bool IsEmpty() const { return ranges_v4_.empty(); }

  bool IsEqualTo(const NetworkAddressMatcher& other) const;

  bool Match(const NetworkAddressRep& addr) const;

 private:
  struct NetworkRangev4 {
    uint32_t min_incl;
    uint32_t max_incl;
  };

  // placeholder for illustration, not implemented.
  struct NetworkRangev6 {
    char min_incl[16];
    char max_incl[16];
  };

  friend void swap(NetworkAddressMatcher& lhs, NetworkAddressMatcher& rhs) {
    using std::swap;
    swap(lhs.ranges_v4_, rhs.ranges_v4_);
  }

  // Sorted, non-overlapping ranges of matching IP addresses.
  std::vector<NetworkRangev4> ranges_v4_;
};

// Returns a compiler library that adds the network functions to the type
// checker.
cel::CompilerLibrary NetworkFunctionsCompilerLibrary();

// Registers the network functions in a runtime for evaluation.
absl::Status RegisterNetworkFunctions(cel::FunctionRegistry& registry,
                                      const cel::RuntimeOptions& options);

// Registers the network types in a runtime for evaluation. This is needed
// for resolving the type name to a runtime type `net.Address != type('foo')`.
absl::Status RegisterNetworkTypes(cel::TypeRegistry& registry,
                                  const cel::RuntimeOptions& options);

}  // namespace cel_codelab

#endif  // THIRD_PARTY_CEL_CPP_CODELAB_NETWORK_FUNCTIONS_H_
