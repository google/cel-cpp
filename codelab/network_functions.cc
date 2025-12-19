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

#include "codelab/network_functions.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "checker/type_checker_builder.h"
#include "common/decl.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/typeinfo.h"
#include "common/value.h"
#include "compiler/compiler.h"
#include "internal/status_macros.h"
#include "runtime/function_adapter.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"
#include "runtime/type_registry.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel_codelab {
namespace {

// TODO(uncreated-issue/86): This is how internal extensions create types, but it isn't
// a good pattern for client extensions (since they can't pool into one eternal
// arena).
google::protobuf::Arena* absl_nonnull BuiltinsArena() {
  static absl::NoDestructor<google::protobuf::Arena> arena;
  return arena.get();
}

cel::Type AddressType() {
  static cel::Type kInstance(
      cel::OpaqueType(BuiltinsArena(), "net.Address", {}));
  return kInstance;
}

cel::Type TypeOfAddressType() {
  static cel::Type kInstance(cel::TypeType(BuiltinsArena(), AddressType()));
  return kInstance;
}

cel::Type AddressMatcherType() {
  static cel::Type kInstance(
      cel::OpaqueType(BuiltinsArena(), "net.AddressMatcher", {}));
  return kInstance;
}

cel::Type TypeOfAddressMatcherType() {
  static cel::Type kInstance(
      cel::TypeType(BuiltinsArena(), AddressMatcherType()));
  return kInstance;
}

absl::StatusOr<IpVersion> ParseAddressImpl(absl::string_view str,
                                           uint32_t* ipv4_out,
                                           absl::Span<char> ipv6_out) {
  if (str.size() < 2 || str.size() > 39) {
    return absl::InvalidArgumentError("unsupported address format (length)");
  }
  if (absl::StrContains(str, ":")) {
    if (ipv6_out.size() < 16) {
      return absl::InternalError("invalid outbuffer in parse call");
    }
    return absl::InvalidArgumentError("unsupported address format (ipv6)");
  }
  uint32_t ipv4 = 0;
  int octet = 0;
  for (auto part : absl::StrSplit(str, '.')) {
    if (octet >= 4) {
      return absl::InvalidArgumentError(
          "unsupported address format (invalid ipv4)");
    }
    int octet_val;
    if (!absl::SimpleAtoi(part, &octet_val) || octet_val > 255 ||
        octet_val < 0) {
      return absl::InvalidArgumentError(
          "unsupported address format (invalid ipv4)");
    }
    ipv4 <<= 8;
    ipv4 |= (uint32_t)octet_val;

    octet++;
  }
  if (octet != 4) {
    return absl::InvalidArgumentError(
        "unsupported address format (invalid ipv4)");
  }
  *ipv4_out = ipv4;
  return IpVersion::kIPv4;
}

absl::Status ConfigureNetworkFunctions(cel::TypeCheckerBuilder& builder) {
  // Type identifiers
  CEL_RETURN_IF_ERROR(builder.AddVariable(
      MakeVariableDecl("net.Address", TypeOfAddressType())));
  CEL_RETURN_IF_ERROR(builder.AddVariable(
      MakeVariableDecl("net.AddressMatcher", TypeOfAddressMatcherType())));
  CEL_RETURN_IF_ERROR(builder.AddVariable(
      MakeVariableDecl("net.addressZeroValue", AddressType())));

  // net.parseAddress(string) -> net.Address
  CEL_ASSIGN_OR_RETURN(
      auto decl,
      MakeFunctionDecl("net.parseAddress",
                       MakeOverloadDecl("net_parseAddress_string",
                                        AddressType(), cel::StringType())));

  CEL_RETURN_IF_ERROR(builder.AddFunction(decl));
  // net.parseAddressOrZero(string) -> net.Address
  CEL_ASSIGN_OR_RETURN(
      decl,
      MakeFunctionDecl("net.parseAddressOrZero",
                       MakeOverloadDecl("net_parseAddressOrZero_string",
                                        AddressType(), cel::StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(decl));

  // net.parseAddressMatcher(string) -> net.AddressMatcher
  CEL_ASSIGN_OR_RETURN(
      decl, MakeFunctionDecl(
                "net.parseAddressMatcher",
                MakeOverloadDecl("net_parseAddressMatcher_string",
                                 AddressMatcherType(), cel::StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(decl));

  // (net.AddressMatcher).containsAddress(net.Address) -> bool
  CEL_ASSIGN_OR_RETURN(
      decl, MakeFunctionDecl(
                "containsAddress",
                MakeMemberOverloadDecl(
                    "net_AddressMatcher_containsAddress_net_Address",
                    cel::BoolType(), AddressMatcherType(), AddressType()),
                MakeMemberOverloadDecl(
                    "net_AddressMatcher_containsAddress_string",
                    cel::BoolType(), AddressMatcherType(), cel::StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(decl));

  return absl::OkStatus();
}

// =============================================================================
// Opaque Value type implementations for NetworkAddressRep.
// =============================================================================

cel::NativeTypeId NetworkAddressRepGetTypeId(
    const cel::OpaqueValueDispatcher* dispatcher,
    cel::OpaqueValueContent content) {
  return cel::TypeId<NetworkAddressRep>();
}

google::protobuf::Arena* absl_nullable NetworkAddressRepGetArena(
    const cel::OpaqueValueDispatcher* absl_nonnull dispatcher,
    cel::OpaqueValueContent content) {
  return nullptr;
}

absl::string_view NetworkAddressRepGetTypeName(
    const cel::OpaqueValueDispatcher* absl_nonnull dispatcher,
    cel::OpaqueValueContent content) {
  return "net.Address";
}

std::string NetworkAddressRepDebugString(
    const cel::OpaqueValueDispatcher* absl_nonnull dispatcher,
    cel::OpaqueValueContent content) {
  return absl::StrCat("net.parseAddress('",
                      content.To<NetworkAddressRep>().Format(), "')");
}

cel::OpaqueType NetworkAddressRepGetRuntimeType(
    const cel::OpaqueValueDispatcher* absl_nonnull dispatcher,
    cel::OpaqueValueContent content) {
  return AddressType().GetOpaque();
}

absl::Status NetworkAddressRepEqual(
    const cel::OpaqueValueDispatcher* absl_nonnull,
    cel::OpaqueValueContent content, const cel::OpaqueValue& other,
    const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull, google::protobuf::Arena* absl_nonnull,
    cel::Value* absl_nonnull result) {
  if (other.GetTypeId() != cel::TypeId<NetworkAddressRep>()) {
    *result = cel::BoolValue(false);
    return absl::OkStatus();
  }
  const NetworkAddressRep rep = content.To<NetworkAddressRep>();
  absl::optional<NetworkAddressRep> other_rep =
      NetworkAddressRep::Unwrap(other);
  ABSL_DCHECK(other_rep.has_value());
  *result = cel::BoolValue(rep.IsEqualTo(*other_rep));
  return absl::OkStatus();
}

cel::OpaqueValue NetworkAddressRepClone(
    const cel::OpaqueValueDispatcher* absl_nonnull,
    cel::OpaqueValueContent content, google::protobuf::Arena* absl_nonnull arena) {
  const NetworkAddressRep* rep = content.To<const NetworkAddressRep*>();
  ABSL_DCHECK(rep != nullptr);
  return NetworkAddressRep::MakeValue(*rep).GetOpaque();
}

// Opaque Value types can be implemented either with a shared dispatcher or
// with a subclass (using vtable dispatch).
//
// We use the shared dispatcher here since the address type has a compact
// representation and we don't need to support different implementations at
// runtime.
//
// If the data structure is more complex, benefits from runtime polymorphism, or
// doesn't have easily defined move, swap, and copy operations, it's
// recommended to use a subclass instead.
static const cel::OpaqueValueDispatcher kAddressDispatcher{
    /*.GetTypeId=*/NetworkAddressRepGetTypeId,
    /*.GetArena=*/NetworkAddressRepGetArena,
    /*.GetTypeName=*/NetworkAddressRepGetTypeName,
    /*.DebugString=*/NetworkAddressRepDebugString,
    /*.GetRuntimeType=*/NetworkAddressRepGetRuntimeType,
    /*.Equal=*/NetworkAddressRepEqual,
    /*.Clone=*/NetworkAddressRepClone};

// =============================================================================
// Opaque Value type implementations for NetworkAddressMatcher.
// =============================================================================

// Implementation of the OpaqueValueInterface for NetworkAddressMatcher.
//
// This is simpler to implement, but adds an extra allocation and pointer
// indirection for every matcher. This is recommended if the data structure is
// more complex.
class NetworkAddressMatcherImpl : public cel::OpaqueValueInterface {
 public:
  explicit NetworkAddressMatcherImpl(NetworkAddressMatcher rep)
      : rep_(std::move(rep)) {}

  const NetworkAddressMatcher& rep() const { return rep_; }

  // implement the OpaqueValueInterface
  std::string DebugString() const final {
    return absl::StrCat("net.ParseAddressMatcher('", "TODO(uncreated-issue/86)", "')");
  }

  absl::string_view GetTypeName() const final { return "net.AddressMatcher"; }

  cel::OpaqueType GetRuntimeType() const final {
    return AddressMatcherType().GetOpaque();
  }

  absl::Status Equal(const cel::OpaqueValue& other,
                     const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                     google::protobuf::MessageFactory* absl_nonnull message_factory,
                     google::protobuf::Arena* absl_nonnull arena,
                     cel::Value* absl_nonnull result) const final {
    if (other.GetTypeId() != cel::TypeId<NetworkAddressMatcherImpl>()) {
      *result = cel::BoolValue(false);
      return absl::OkStatus();
    }
    const NetworkAddressMatcherImpl* other_rep =
        static_cast<const NetworkAddressMatcherImpl*>(other.interface());
    *result = cel::BoolValue(rep_.IsEqualTo(other_rep->rep_));
    return absl::OkStatus();
  }

  cel::OpaqueValue Clone(google::protobuf::Arena* absl_nonnull arena) const final {
    return NetworkAddressMatcher::MakeValue(arena, rep_).GetOpaque();
  }

  cel::NativeTypeId GetNativeTypeId() const final {
    return cel::TypeId<NetworkAddressMatcherImpl>();
  }

 private:
  NetworkAddressMatcher rep_;
};

// =============================================================================
// Extension function implementations.
// =============================================================================
cel::Value parseAddress(
    const cel::StringValue& str,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  std::string buf;
  absl::string_view addr = str.ToStringView(&buf);
  absl::optional<NetworkAddressRep> rep = NetworkAddressRep::Parse(addr);
  if (!rep.has_value()) {
    return cel::ErrorValue(absl::InvalidArgumentError("invalid address"));
  }
  return NetworkAddressRep::MakeValue(*rep);
}

cel::Value parseAddressOrZero(const cel::StringValue& str) {
  std::string buf;
  absl::string_view addr = str.ToStringView(&buf);
  absl::optional<NetworkAddressRep> rep = NetworkAddressRep::Parse(addr);
  static const NetworkAddressRep kZero;
  if (!rep.has_value()) {
    return NetworkAddressRep::MakeValue(kZero);
  }
  return NetworkAddressRep::MakeValue(*rep);
}

cel::Value parseAddressMatcher(
    const cel::StringValue& str,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  std::string buf;
  absl::string_view addr = str.ToStringView(&buf);
  absl::optional<NetworkAddressMatcher> rep =
      NetworkAddressMatcher::Parse(addr);
  if (!rep.has_value()) {
    return cel::ErrorValue(
        absl::InvalidArgumentError("invalid address matcher"));
  }

  return NetworkAddressMatcher::MakeValue(arena, std::move(rep).value());
}

cel::Value containsAddress(const cel::OpaqueValue& matcher,
                           const cel::OpaqueValue& addr) {
  const auto* matcher_rep = NetworkAddressMatcher::Unwrap(matcher);
  auto addr_rep = NetworkAddressRep::Unwrap(addr);
  if (matcher_rep == nullptr || !addr_rep.has_value()) {
    // dispatcher should catch this, but right now only distiguishes at the
    // kind level.
    return cel::ErrorValue(absl::InvalidArgumentError("no matching overload"));
  }
  return cel::BoolValue(matcher_rep->Match(*addr_rep));
}

}  // namespace

cel::Value NetworkAddressRep::MakeValue(const NetworkAddressRep& rep) {
  return UnsafeOpaqueValue(&kAddressDispatcher,
                           cel::OpaqueValueContent::From(rep));
}

absl::optional<NetworkAddressRep> NetworkAddressRep::Unwrap(
    const cel::Value& value) {
  auto opaque = value.AsOpaque();
  if (!opaque.has_value() ||
      opaque->GetTypeId() != cel::TypeId<NetworkAddressRep>()) {
    return absl::nullopt;
  }

  // Note: safety depends on:
  // 1) correctly implementing GetTypeId
  // 2) the TypeId is unique
  // 3) all calls to UnsafeOpaqueValue with the dispatcher provide the expected
  //    content type.
  return opaque->content().To<NetworkAddressRep>();
}

absl::optional<NetworkAddressRep> NetworkAddressRep::Parse(
    absl::string_view str) {
  uint32_t ipv4 = 0;
  char ipv6[16];
  auto version = ParseAddressImpl(str, &ipv4, ipv6);
  if (!version.ok()) {
    return absl::nullopt;
  }
  if (*version != IpVersion::kIPv4) {
    return absl::nullopt;
  }
  NetworkAddressRep rep;
  rep.version_ = *version;
  rep.addr_.v4 = ipv4;
  return rep;
}

bool NetworkAddressRep::IsEqualTo(const NetworkAddressRep& other) const {
  if (version_ != other.version_) {
    return false;
  }
  if (version_ == IpVersion::kIPv4) {
    return addr_.v4 == other.addr_.v4;
  }
  return false;
}

bool NetworkAddressRep::IsLessThan(const NetworkAddressRep& other) const {
  if (version_ != other.version_) {
    return version_ < other.version_;
  }
  if (version_ == IpVersion::kIPv4) {
    return addr_.v4 < other.addr_.v4;
  }
  return false;
}

absl::optional<NetworkAddressMatcher> NetworkAddressMatcher::Parse(
    absl::string_view str) {
  // range style addr-addr
  int dash_pos = str.find('-');
  if (dash_pos == absl::string_view::npos) {
    // TODO(uncreated-issue/86):  CIDR style addr/prefix-length
    return absl::nullopt;
  }
  absl::string_view min_str = str.substr(0, dash_pos);
  absl::string_view max_str = str.substr(dash_pos + 1);

  NetworkRangev4 v4;
  NetworkRangev6 v6;
  auto min_parse = ParseAddressImpl(min_str, &v4.min_incl, v6.min_incl);
  if (!min_parse.ok()) {
    return absl::nullopt;
  }
  auto max_parse = ParseAddressImpl(max_str, &v4.max_incl, v6.max_incl);
  if (!max_parse.ok()) {
    return absl::nullopt;
  }
  if (*min_parse != *max_parse) {
    return absl::nullopt;
  }
  NetworkAddressMatcher rep;
  if (*min_parse == IpVersion::kIPv4) {
    if (v4.min_incl > v4.max_incl) {
      return absl::nullopt;
    }
    rep.ranges_v4_.push_back(v4);
  } else if (*min_parse == IpVersion::kIPv6) {
    return absl::nullopt;
  }

  return rep;
}

cel::Value NetworkAddressMatcher::MakeValue(google::protobuf::Arena* arena,
                                            NetworkAddressMatcher rep) {
  auto* iface =
      google::protobuf::Arena::Create<NetworkAddressMatcherImpl>(arena, std::move(rep));

  return cel::OpaqueValue(iface, arena);
}

const NetworkAddressMatcher* NetworkAddressMatcher::Unwrap(
    const cel::Value& value) {
  auto opaque = value.AsOpaque();
  if (!opaque.has_value() || opaque->interface() == nullptr ||
      opaque->GetTypeId() != cel::TypeId<NetworkAddressMatcherImpl>()) {
    return nullptr;
  }
  // Note: the safety of down casting like this depends on guaranteeing the
  // GetTypeId implementation is correct and is a unique ID. The CEL runtime
  // does not inspect or modify the interface type outside calling the interface
  // member functions.
  return &(static_cast<const NetworkAddressMatcherImpl*>(opaque->interface())
               ->rep());
}

bool NetworkAddressMatcher::Match(const NetworkAddressRep& addr) const {
  if (addr.IsZeroValue()) {
    return false;
  }
  if (addr.IsIPv4()) {
    for (const auto& range : ranges_v4_) {
      if (addr.GetIPv4() >= range.min_incl &&
          addr.GetIPv4() <= range.max_incl) {
        return true;
      }
    }
  }

  // TODO(uncreated-issue/86): ipv6 support
  return false;
}

bool NetworkAddressMatcher::IsEqualTo(
    const NetworkAddressMatcher& other) const {
  if (ranges_v4_.size() != other.ranges_v4_.size()) {
    return false;
  }
  for (int i = 0; i < ranges_v4_.size(); ++i) {
    if (ranges_v4_[i].min_incl != other.ranges_v4_[i].min_incl ||
        ranges_v4_[i].max_incl != other.ranges_v4_[i].max_incl) {
      return false;
    }
  }
  return true;
}

cel::CompilerLibrary NetworkFunctionsCompilerLibrary() {
  return cel::CompilerLibrary("cel_codelab.net", ConfigureNetworkFunctions);
}

absl::Status RegisterNetworkTypes(cel::TypeRegistry& registry,
                                  const cel::RuntimeOptions& options) {
  CEL_RETURN_IF_ERROR(registry.RegisterType(AddressType().GetOpaque()));
  CEL_RETURN_IF_ERROR(registry.RegisterType(AddressMatcherType().GetOpaque()));
  return absl::OkStatus();
}

absl::Status RegisterNetworkFunctions(cel::FunctionRegistry& registry,
                                      const cel::RuntimeOptions& options) {
  // TODO(uncreated-issue/86): remaining functions
  auto s = cel::UnaryFunctionAdapter<cel::Value, const cel::StringValue&>::
      RegisterGlobalOverload("net.parseAddress", &parseAddress, registry);
  s.Update(cel::UnaryFunctionAdapter<cel::Value, const cel::StringValue&>::
               RegisterGlobalOverload("net.parseAddressOrZero",
                                      &parseAddressOrZero, registry));

  s.Update(cel::UnaryFunctionAdapter<cel::Value, const cel::StringValue&>::
               RegisterGlobalOverload("net.parseAddressMatcher",
                                      &parseAddressMatcher, registry));
  s.Update(cel::BinaryFunctionAdapter<
           cel::Value, const cel::OpaqueValue&,
           const cel::OpaqueValue&>::RegisterMemberOverload("containsAddress",
                                                            &containsAddress,
                                                            registry));
  return s;
}

}  // namespace cel_codelab
