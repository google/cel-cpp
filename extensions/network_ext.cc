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

#include "extensions/network_ext.h"

#include <string>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/builtins.h"
#include "checker/type_checker_builder.h"
#include "common/decl.h"
#include "common/ipaddress_oss.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/value.h"
#include "compiler/compiler.h"
#include "extensions/network_ext_functions.h"
#include "internal/status_macros.h"
#include "runtime/function.h"
#include "runtime/function_adapter.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"
#include "runtime/type_registry.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::extensions {
namespace {

using ::cel::BinaryFunctionAdapter;
using ::cel::BoolValue;
using ::cel::IPAddress;
using ::cel::MakeFunctionDecl;
using ::cel::MakeMemberOverloadDecl;
using ::cel::MakeOverloadDecl;
using ::cel::NativeTypeId;
using ::cel::OpaqueType;
using ::cel::OpaqueValue;
using ::cel::OpaqueValueContent;
using ::cel::OpaqueValueDispatcher;
using ::cel::StringValue;
using ::cel::Type;
using ::cel::TypeType;
using ::cel::UnaryFunctionAdapter;
using ::cel::UnsafeOpaqueValue;
using ::cel::Value;
using ::cel::builtin::kEqual;
using ::cel::builtin::kString;
using ::google::protobuf::Arena;
using ::google::protobuf::DescriptorPool;
using ::google::protobuf::MessageFactory;

// Arena for static type instances
Arena* absl_nonnull BuiltinsArena() {
  static absl::NoDestructor<Arena> arena;
  return arena.get();
}

// CEL Type Declarations
OpaqueType IpType() {
  static const absl::NoDestructor<OpaqueType> kInstance(
      BuiltinsArena(), "net.IP", std::vector<Type>{});
  return *kInstance;
}

Type TypeOfIpType() {
  static const absl::NoDestructor<Type> kInstance(
      TypeType(BuiltinsArena(), IpType()));
  return *kInstance;
}

OpaqueType CidrType() {
  static const absl::NoDestructor<OpaqueType> kInstance(
      BuiltinsArena(), "net.CIDR", std::vector<Type>{});
  return *kInstance;
}

Type TypeOfCidrType() {
  static const absl::NoDestructor<Type> kInstance(
      TypeType(BuiltinsArena(), CidrType()));
  return *kInstance;
}

// -----------------------------------------------------------------------------
// Dispatcher for IpAddrRep (net.IP)
// -----------------------------------------------------------------------------

NativeTypeId IpAddrRep_GetTypeId(const OpaqueValueDispatcher*,
                                 OpaqueValueContent content) {
  return IpAddrRep::GetTypeId();
}

absl::string_view IpAddrRep_GetTypeName(const OpaqueValueDispatcher*,
                                        OpaqueValueContent content) {
  return "net.IP";
}

std::string IpAddrRep_DebugString(const OpaqueValueDispatcher*,
                                  OpaqueValueContent content) {
  return content.To<const IpAddrRep*>()->DebugString();
}

absl::Status IpAddrRep_Equal(const OpaqueValueDispatcher*,
                             OpaqueValueContent content,
                             const OpaqueValue& other, const DescriptorPool*,
                             MessageFactory*, Arena*, Value* result) {
  const IpAddrRep* self = content.To<const IpAddrRep*>();
  const IpAddrRep* other_rep = IpAddrRep::Unwrap(other);
  if (!other_rep) {
    *result = BoolValue(false);
    return absl::OkStatus();
  }
  *result = BoolValue(self->Equals(*other_rep));
  return absl::OkStatus();
}

OpaqueValue IpAddrRep_Clone(const OpaqueValueDispatcher*,
                            OpaqueValueContent content, Arena* arena) {
  const IpAddrRep* self = content.To<const IpAddrRep*>();
  return IpAddrRep::Create(arena, self->addr()).GetOpaque();
}

OpaqueType IpAddrRep_GetRuntimeType(const OpaqueValueDispatcher*,
                                    OpaqueValueContent) {
  return IpType();
}

static const OpaqueValueDispatcher kIpAddrRepDispatcher = {
    .get_type_id = IpAddrRep_GetTypeId,
    .get_arena = nullptr,
    .get_type_name = IpAddrRep_GetTypeName,
    .debug_string = IpAddrRep_DebugString,
    .get_runtime_type = IpAddrRep_GetRuntimeType,
    .equal = IpAddrRep_Equal,
    .clone = IpAddrRep_Clone,
};

// -----------------------------------------------------------------------------
// Dispatcher for CidrRangeRep (net.CIDR)
// -----------------------------------------------------------------------------

NativeTypeId CidrRangeRep_GetTypeId(const OpaqueValueDispatcher*,
                                    OpaqueValueContent content) {
  return CidrRangeRep::GetTypeId();
}

absl::string_view CidrRangeRep_GetTypeName(const OpaqueValueDispatcher*,
                                           OpaqueValueContent content) {
  return "net.CIDR";
}

std::string CidrRangeRep_DebugString(const OpaqueValueDispatcher*,
                                     OpaqueValueContent content) {
  return content.To<const CidrRangeRep*>()->DebugString();
}

absl::Status CidrRangeRep_Equal(const OpaqueValueDispatcher*,
                                OpaqueValueContent content,
                                const OpaqueValue& other, const DescriptorPool*,
                                MessageFactory*, Arena*, Value* result) {
  const CidrRangeRep* self = content.To<const CidrRangeRep*>();
  const CidrRangeRep* other_rep = CidrRangeRep::Unwrap(other);
  if (!other_rep) {
    *result = BoolValue(false);
    return absl::OkStatus();
  }
  *result = BoolValue(self->Equals(*other_rep));
  return absl::OkStatus();
}

OpaqueValue CidrRangeRep_Clone(const OpaqueValueDispatcher*,
                               OpaqueValueContent content, Arena* arena) {
  const CidrRangeRep* self = content.To<const CidrRangeRep*>();
  return CidrRangeRep::Create(arena, self->host(), self->length()).GetOpaque();
}

OpaqueType CidrRangeRep_GetRuntimeType(const OpaqueValueDispatcher*,
                                       OpaqueValueContent) {
  return CidrType();
}

static const OpaqueValueDispatcher kCidrRangeRepDispatcher = {
    .get_type_id = CidrRangeRep_GetTypeId,
    .get_arena = nullptr,
    .get_type_name = CidrRangeRep_GetTypeName,
    .debug_string = CidrRangeRep_DebugString,
    .get_runtime_type = CidrRangeRep_GetRuntimeType,
    .equal = CidrRangeRep_Equal,
    .clone = CidrRangeRep_Clone,
};

}  // namespace

// -----------------------------------------------------------------------------
// IpAddrRep Method Implementations
// -----------------------------------------------------------------------------
Value IpAddrRep::Create(Arena* arena, const IPAddress& addr) {
  IpAddrRep* rep = Arena::Create<IpAddrRep>(arena, addr);
  return UnsafeOpaqueValue(&kIpAddrRepDispatcher,
                           OpaqueValueContent::From(rep));
}

const IpAddrRep* IpAddrRep::Unwrap(const Value& value) {
  auto opaque = value.AsOpaque();
  if (!opaque.has_value() || opaque->GetTypeId() != IpAddrRep::GetTypeId()) {
    return nullptr;
  }
  return opaque->content().To<const IpAddrRep*>();
}

std::string IpAddrRep::DebugString() const {
  if (!IsInitializedAddress(addr_)) {
    return "ip(<uninitialized>)";
  }
  return absl::StrCat("ip('", addr_.ToString(), "')");
}

// -----------------------------------------------------------------------------
// CidrRangeRep Method Implementations
// -----------------------------------------------------------------------------
Value CidrRangeRep::Create(Arena* arena, const IPAddress& host,
                           int length) {  // Changed signature
  CidrRangeRep* rep = Arena::Create<CidrRangeRep>(
      arena, host, length);  // Changed constructor call
  return UnsafeOpaqueValue(&kCidrRangeRepDispatcher,
                           OpaqueValueContent::From(rep));
}

const CidrRangeRep* CidrRangeRep::Unwrap(const Value& value) {
  auto opaque = value.AsOpaque();
  if (!opaque.has_value() || opaque->GetTypeId() != CidrRangeRep::GetTypeId()) {
    return nullptr;
  }
  return opaque->content().To<const CidrRangeRep*>();
}

std::string CidrRangeRep::DebugString() const {
  if (!IsInitializedAddress(host_) ||
      length_ < 0) {  // Changed to use host_ and length_
    return "cidr(<uninitialized>)";
  }
  return absl::StrCat("cidr('", host_.ToString(), "/", length_,
                      "')");  // Changed to use host_ and length_
}

// -----------------------------------------------------------------------------
// CEL Extension Registration
// -----------------------------------------------------------------------------

absl::Status ConfigureNetworkFunctions(cel::TypeCheckerBuilder& builder) {
  // Register Type Identifiers
  CEL_RETURN_IF_ERROR(
      builder.AddVariable(cel::MakeVariableDecl("net.IP", TypeOfIpType())));
  CEL_RETURN_IF_ERROR(
      builder.AddVariable(cel::MakeVariableDecl("net.CIDR", TypeOfCidrType())));

  CEL_ASSIGN_OR_RETURN(
      auto decl_is_ip,
      MakeFunctionDecl("isIP", MakeOverloadDecl("is_ip_string", cel::BoolType(),
                                                cel::StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(decl_is_ip));

  CEL_ASSIGN_OR_RETURN(
      auto decl_ip,
      MakeFunctionDecl(
          "ip", MakeOverloadDecl("string_to_ip", IpType(), cel::StringType()),
          MakeMemberOverloadDecl("cidr_ip", IpType(), CidrType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(decl_ip));

  CEL_ASSIGN_OR_RETURN(
      auto decl_is_cidr,
      MakeFunctionDecl("isCIDR",
                       MakeOverloadDecl("is_cidr_string", cel::BoolType(),
                                        cel::StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(decl_is_cidr));

  CEL_ASSIGN_OR_RETURN(
      auto decl_cidr,
      MakeFunctionDecl("cidr", MakeOverloadDecl("string_to_cidr", CidrType(),
                                                cel::StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(decl_cidr));

  CEL_ASSIGN_OR_RETURN(
      auto decl_ip_is_canonical,
      MakeFunctionDecl("ip.isCanonical",
                       MakeOverloadDecl("ip_is_canonical_string",
                                        cel::BoolType(), cel::StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(decl_ip_is_canonical));

  CEL_ASSIGN_OR_RETURN(
      auto decl_ip_family,
      MakeFunctionDecl("family", MakeMemberOverloadDecl(
                                     "ip_family", cel::IntType(), IpType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(decl_ip_family));

  CEL_ASSIGN_OR_RETURN(
      auto decl_ip_is_loopback,
      MakeFunctionDecl(
          "isLoopback",
          MakeMemberOverloadDecl("ip_is_loopback", cel::BoolType(), IpType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(decl_ip_is_loopback));

  CEL_ASSIGN_OR_RETURN(
      auto decl_ip_is_global_unicast,
      MakeFunctionDecl("isGlobalUnicast",
                       MakeMemberOverloadDecl("ip_is_global_unicast",
                                              cel::BoolType(), IpType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(decl_ip_is_global_unicast));

  CEL_ASSIGN_OR_RETURN(
      auto decl_ip_is_link_local_multicast,
      MakeFunctionDecl("isLinkLocalMulticast",
                       MakeMemberOverloadDecl("ip_is_link_local_multicast",
                                              cel::BoolType(), IpType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(decl_ip_is_link_local_multicast));

  CEL_ASSIGN_OR_RETURN(
      auto decl_ip_is_link_local_unicast,
      MakeFunctionDecl("isLinkLocalUnicast",
                       MakeMemberOverloadDecl("ip_is_link_local_unicast",
                                              cel::BoolType(), IpType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(decl_ip_is_link_local_unicast));

  CEL_ASSIGN_OR_RETURN(
      auto decl_ip_is_unspecified,
      MakeFunctionDecl("isUnspecified",
                       MakeMemberOverloadDecl("ip_is_unspecified",
                                              cel::BoolType(), IpType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(decl_ip_is_unspecified));

  CEL_ASSIGN_OR_RETURN(
      auto decl_cidr_contains_ip,
      MakeFunctionDecl(
          "containsIP",
          MakeMemberOverloadDecl("cidr_contains_ip_ip", cel::BoolType(),
                                 CidrType(), IpType()),
          MakeMemberOverloadDecl("cidr_contains_ip_string", cel::BoolType(),
                                 CidrType(), cel::StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(decl_cidr_contains_ip));

  CEL_ASSIGN_OR_RETURN(
      auto decl_cidr_contains_cidr,
      MakeFunctionDecl(
          "containsCIDR",
          MakeMemberOverloadDecl("cidr_contains_cidr_cidr", cel::BoolType(),
                                 CidrType(), CidrType()),
          MakeMemberOverloadDecl("cidr_contains_cidr_string", cel::BoolType(),
                                 CidrType(), cel::StringType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(decl_cidr_contains_cidr));

  CEL_ASSIGN_OR_RETURN(
      auto decl_cidr_masked,
      MakeFunctionDecl("masked", MakeMemberOverloadDecl(
                                     "cidr_masked", CidrType(), CidrType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(decl_cidr_masked));

  CEL_ASSIGN_OR_RETURN(
      auto decl_cidr_prefix_length,
      MakeFunctionDecl("prefixLength",
                       MakeMemberOverloadDecl("cidr_prefix_length",
                                              cel::IntType(), CidrType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(decl_cidr_prefix_length));

  CEL_ASSIGN_OR_RETURN(
      auto decl_string,
      MakeFunctionDecl(
          kString,
          MakeMemberOverloadDecl("ip_to_string", cel::StringType(), IpType()),
          MakeMemberOverloadDecl("cidr_to_string", cel::StringType(),
                                 CidrType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(decl_string));

  // Add Equality Operator overloads for net.IP and net.CIDR
  CEL_ASSIGN_OR_RETURN(
      auto decl_equals,
      MakeFunctionDecl(
          kEqual,
          MakeOverloadDecl("ip_equal_ip", cel::BoolType(), IpType(), IpType()),
          MakeOverloadDecl("cidr_equal_cidr", cel::BoolType(), CidrType(),
                           CidrType())));
  CEL_RETURN_IF_ERROR(builder.AddFunction(decl_equals));

  return absl::OkStatus();
}

cel::CompilerLibrary NetworkCompilerLibrary() {
  return cel::CompilerLibrary("cel.extensions.network",
                              ConfigureNetworkFunctions);
}

absl::Status RegisterNetworkTypes(cel::TypeRegistry& registry,
                                  const cel::RuntimeOptions& options) {
  CEL_RETURN_IF_ERROR(registry.RegisterType(IpType()));
  CEL_RETURN_IF_ERROR(registry.RegisterType(CidrType()));
  return absl::OkStatus();
}
// Implementation for Opaque type equality
Value OpaqueEq(const Value& v1, const Value& v2,
               const Function::InvokeContext& context) {
  Value result;
  absl::Status status =
      v1.Equal(v2, context.descriptor_pool(), context.message_factory(),
               context.arena(), &result);
  if (!status.ok()) {
    // This shouldn't happen if the types are supported by the dispatcher
    return ErrorValue(status);
  }
  return result;
}

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

// Registers network functions with the function registry.
absl::Status RegisterNetworkFunctions(cel::FunctionRegistry& registry,
                                      const cel::RuntimeOptions& options) {
  // ... other function registrations ...
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, const StringValue&>::CreateDescriptor("isIP",
                                                                        false),
      UnaryFunctionAdapter<Value, const StringValue&>::WrapFunction(&NetIsIP)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, const StringValue&>::CreateDescriptor("ip",
                                                                        false),
      UnaryFunctionAdapter<Value, const StringValue&>::WrapFunction(
          &NetIPString)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, const StringValue&>::CreateDescriptor(
          "isCIDR", false),
      UnaryFunctionAdapter<Value, const StringValue&>::WrapFunction(
          &NetIsCIDR)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, const StringValue&>::CreateDescriptor("cidr",
                                                                        false),
      UnaryFunctionAdapter<Value, const StringValue&>::WrapFunction(
          &NetCIDRString)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, const StringValue&>::CreateDescriptor(
          "ip.isCanonical", false),
      UnaryFunctionAdapter<Value, const StringValue&>::WrapFunction(
          &NetIPIsCanonical)));

  // Register Member Functions for net.IP
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, const OpaqueValue&>::CreateDescriptor(
          "family", true),
      UnaryFunctionAdapter<Value, const OpaqueValue&>::WrapFunction(
          &NetIPFamily)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, const OpaqueValue&>::CreateDescriptor(
          "isLoopback", true),
      UnaryFunctionAdapter<Value, const OpaqueValue&>::WrapFunction(
          &NetIPIsLoopback)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, const OpaqueValue&>::CreateDescriptor(
          "isGlobalUnicast", true),
      UnaryFunctionAdapter<Value, const OpaqueValue&>::WrapFunction(
          &NetIPIsGlobalUnicast)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, const OpaqueValue&>::CreateDescriptor(
          "isLinkLocalMulticast", true),
      UnaryFunctionAdapter<Value, const OpaqueValue&>::WrapFunction(
          &NetIPIsLinkLocalMulticast)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, const OpaqueValue&>::CreateDescriptor(
          "isLinkLocalUnicast", true),
      UnaryFunctionAdapter<Value, const OpaqueValue&>::WrapFunction(
          &NetIPIsLinkLocalUnicast)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, const OpaqueValue&>::CreateDescriptor(
          "isUnspecified", true),
      UnaryFunctionAdapter<Value, const OpaqueValue&>::WrapFunction(
          &NetIPIsUnspecified)));

  // Register Member Functions for net.CIDR
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, const OpaqueValue&,
                            const OpaqueValue&>::CreateDescriptor("containsIP",
                                                                  true),
      BinaryFunctionAdapter<Value, const OpaqueValue&, const OpaqueValue&>::
          WrapFunction(&NetCIDRContainsIP)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, const OpaqueValue&,
                            const StringValue&>::CreateDescriptor("containsIP",
                                                                  true),
      BinaryFunctionAdapter<Value, const OpaqueValue&, const StringValue&>::
          WrapFunction(&NetCIDRContainsIPString)));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, const OpaqueValue&, const OpaqueValue&>::
          CreateDescriptor("containsCIDR", true),
      BinaryFunctionAdapter<Value, const OpaqueValue&, const OpaqueValue&>::
          WrapFunction(&NetCIDRContainsCIDR)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, const OpaqueValue&, const StringValue&>::
          CreateDescriptor("containsCIDR", true),
      BinaryFunctionAdapter<Value, const OpaqueValue&, const StringValue&>::
          WrapFunction(&NetCIDRContainsCIDRString)));

  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, const OpaqueValue&>::CreateDescriptor("ip",
                                                                        true),
      UnaryFunctionAdapter<Value, const OpaqueValue&>::WrapFunction(
          &NetCIDRIP)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, const OpaqueValue&>::CreateDescriptor(
          "masked", true),
      UnaryFunctionAdapter<Value, const OpaqueValue&>::WrapFunction(
          &NetCIDRMasked)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, const OpaqueValue&>::CreateDescriptor(
          "prefixLength", true),
      UnaryFunctionAdapter<Value, const OpaqueValue&>::WrapFunction(
          &NetCIDRPrefixLength)));

  // Register the combined string function
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, const OpaqueValue&>::CreateDescriptor(kString,
                                                                        true),
      UnaryFunctionAdapter<Value, const OpaqueValue&>::WrapFunction(
          &NetToString)));

  // Register equality for IP and CIDR
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, const OpaqueValue&,
                            const OpaqueValue&>::CreateDescriptor(kEqual,
                                                                  false),
      BinaryFunctionAdapter<Value, const OpaqueValue&,
                            const OpaqueValue&>::WrapFunction(&OpaqueEq)));

  return absl::OkStatus();
}

}  // namespace cel::extensions
