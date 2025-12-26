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

#include <memory>
#include <utility>

#include "cel/expr/syntax.pb.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/ast.h"
#include "common/minimal_descriptor_pool.h"
#include "common/value.h"
#include "common/values/bool_value.h"
#include "common/values/error_value.h"
#include "common/values/int_value.h"
#include "common/values/string_value.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "runtime/activation.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_builder_factory.h"
#include "runtime/standard_functions.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"

// Includes for Compiler
#include "checker/validation_result.h"
#include "compiler/compiler.h"
#include "compiler/compiler_factory.h"
#include "runtime/runtime_options.h"

namespace cel::extensions {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::cel::Activation;
using ::testing::Eq;
using ::testing::HasSubstr;

class NetworkExtTest : public ::testing::Test {
 protected:
  NetworkExtTest() = default;

  void SetUp() override {
    // 1. Configure the Compiler
    auto compiler_builder =
        cel::NewCompilerBuilder(cel::GetMinimalDescriptorPool());
    ASSERT_THAT(compiler_builder.status(), IsOk());
    ASSERT_THAT((*compiler_builder)->AddLibrary(NetworkCompilerLibrary()),
                IsOk());
    ASSERT_OK_AND_ASSIGN(compiler_, std::move(*compiler_builder)->Build());

    // 2. Configure the Modern Runtime
    cel::RuntimeOptions runtime_options;
    // Wrap the raw pointer in a std::shared_ptr with a NO-OP DELETER
    std::shared_ptr<const google::protobuf::DescriptorPool> descriptor_pool(
        cel::GetMinimalDescriptorPool(), [](const google::protobuf::DescriptorPool*) {
          // Do nothing, as the pool is static.
        });

    auto runtime_builder =
        cel::CreateRuntimeBuilder(descriptor_pool, runtime_options);
    ASSERT_THAT(runtime_builder.status(),
                IsOk());  // Check if CreateRuntimeBuilder succeeded

    ASSERT_THAT(
        RegisterNetworkTypes(runtime_builder->type_registry(), runtime_options),
        IsOk());
    ASSERT_THAT(RegisterNetworkFunctions(runtime_builder->function_registry(),
                                         runtime_options),
                IsOk());

    ASSERT_THAT(cel::RegisterStandardFunctions(
                    runtime_builder->function_registry(), runtime_options),
                IsOk());

    // Build the runtime
    ASSERT_OK_AND_ASSIGN(runtime_, std::move(*runtime_builder).Build());
  }

  // ... Evaluate() function and member variables ...
  absl::StatusOr<cel::Value> Evaluate(absl::string_view expr) {
    auto validation_result = compiler_->Compile(expr);
    CEL_RETURN_IF_ERROR(validation_result.status());

    if (!validation_result->GetIssues().empty()) {
      return absl::InvalidArgumentError(
          validation_result->GetIssues()[0].message());
    }

    if (!validation_result->IsValid()) {
      return absl::InternalError(
          "Compilation produced an invalid AST without issues.");
    }

    CEL_ASSIGN_OR_RETURN(std::unique_ptr<cel::Ast> ast,
                         validation_result->ReleaseAst());

    if (ast == nullptr) {
      return absl::InternalError("ValidationResult returned a null AST.");
    }

    CEL_ASSIGN_OR_RETURN(auto program, runtime_->CreateProgram(std::move(ast)));

    Activation activation;
    return program->Evaluate(&arena_, activation);
  }

  std::unique_ptr<cel::Compiler> compiler_;
  std::unique_ptr<cel::Runtime> runtime_;
  google::protobuf::Arena arena_;
};

// --- Global Checks (isIP, isCIDR) ---
TEST_F(NetworkExtTest, IsIPValidIPv4) {
  ASSERT_OK_AND_ASSIGN(auto value, Evaluate("isIP('1.2.3.4')"));
  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.As<cel::BoolValue>()->NativeValue(), Eq(true));
}

TEST_F(NetworkExtTest, IsIPValidIPv6) {
  ASSERT_OK_AND_ASSIGN(auto value, Evaluate("isIP('2001:db8::1')"));
  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.As<cel::BoolValue>()->NativeValue(), Eq(true));
}

TEST_F(NetworkExtTest, IsIPInvalid) {
  ASSERT_OK_AND_ASSIGN(auto value, Evaluate("isIP('not.an.ip')"));
  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.As<cel::BoolValue>()->NativeValue(), Eq(false));
}

TEST_F(NetworkExtTest, IsIPWithPort) {
  ASSERT_OK_AND_ASSIGN(auto value, Evaluate("isIP('127.0.0.1:80')"));
  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.As<cel::BoolValue>()->NativeValue(), Eq(false));
}

TEST_F(NetworkExtTest, IsCIDRValid) {
  ASSERT_OK_AND_ASSIGN(auto value, Evaluate("isCIDR('10.0.0.0/8')"));
  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.As<cel::BoolValue>()->NativeValue(), Eq(true));
}

TEST_F(NetworkExtTest, IsCIDRInvalidMask) {
  ASSERT_OK_AND_ASSIGN(auto value, Evaluate("isCIDR('10.0.0.0/999')"));
  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.As<cel::BoolValue>()->NativeValue(), Eq(false));
}

// --- IP Constructors & Equality ---
TEST_F(NetworkExtTest, IPEqualityIPv4) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       Evaluate("ip('127.0.0.1') == ip('127.0.0.1')"));
  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.As<cel::BoolValue>()->NativeValue(), Eq(true));
}

TEST_F(NetworkExtTest, IPInequality) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       Evaluate("ip('127.0.0.1') == ip('1.2.3.4')"));
  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.As<cel::BoolValue>()->NativeValue(), Eq(false));
}

TEST_F(NetworkExtTest, IPEqualityIPv6MixedCase) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       Evaluate("ip('2001:db8::1') == ip('2001:DB8::1')"));
  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.As<cel::BoolValue>()->NativeValue(), Eq(true));
}

// --- String Conversion ---
TEST_F(NetworkExtTest, IPToStringIPv4) {
  ASSERT_OK_AND_ASSIGN(auto value, Evaluate("ip('1.2.3.4').string()"));
  ASSERT_TRUE(value.IsString());
  EXPECT_THAT(value.As<cel::StringValue>()->ToString(), Eq("1.2.3.4"));
}

TEST_F(NetworkExtTest, IPToStringIPv6) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       Evaluate("ip('2001:db8::1').string()"));  // .string()
  ASSERT_TRUE(value.IsString());
  EXPECT_THAT(value.As<cel::StringValue>()->ToString(), Eq("2001:db8::1"));
}

TEST_F(NetworkExtTest, CIDRToStringIPv4) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       Evaluate("cidr('10.0.0.0/8').string()"));  // .string()
  ASSERT_TRUE(value.IsString());
  EXPECT_THAT(value.As<cel::StringValue>()->ToString(), Eq("10.0.0.0/8"));
}

TEST_F(NetworkExtTest, CIDRToStringIPv6) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       Evaluate("cidr('::1/128').string()"));  // .string()
  ASSERT_TRUE(value.IsString());
  EXPECT_THAT(value.As<cel::StringValue>()->ToString(), Eq("::1/128"));
}

// --- Family ---
TEST_F(NetworkExtTest, FamilyIPv4) {
  ASSERT_OK_AND_ASSIGN(auto value, Evaluate("ip('127.0.0.1').family()"));
  ASSERT_TRUE(value.IsInt());
  EXPECT_THAT(value.As<cel::IntValue>()->NativeValue(), Eq(4));
}

TEST_F(NetworkExtTest, FamilyIPv6) {
  ASSERT_OK_AND_ASSIGN(auto value, Evaluate("ip('::1').family()"));
  ASSERT_TRUE(value.IsInt());
  EXPECT_THAT(value.As<cel::IntValue>()->NativeValue(), Eq(6));
}

// --- Canonicalization ---
TEST_F(NetworkExtTest, IsCanonicalIPv4) {
  ASSERT_OK_AND_ASSIGN(auto value, Evaluate("ip.isCanonical('127.0.0.1')"));
  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.As<cel::BoolValue>()->NativeValue(), Eq(true));
}

TEST_F(NetworkExtTest, IsCanonicalIPv6) {
  ASSERT_OK_AND_ASSIGN(auto value, Evaluate("ip.isCanonical('2001:db8::1')"));
  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.As<cel::BoolValue>()->NativeValue(), Eq(true));
}

TEST_F(NetworkExtTest, IsCanonicalIPv6Uppercase) {
  ASSERT_OK_AND_ASSIGN(auto value, Evaluate("ip.isCanonical('2001:DB8::1')"));
  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.As<cel::BoolValue>()->NativeValue(), Eq(false));
}

TEST_F(NetworkExtTest, IsCanonicalIPv6Expanded) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       Evaluate("ip.isCanonical('2001:db8:0:0:0:0:0:1')"));
  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.As<cel::BoolValue>()->NativeValue(), Eq(false));
}

// --- IP Types (Loopback, Unspecified, etc) ---
TEST_F(NetworkExtTest, IsLoopbackIPv4) {
  ASSERT_OK_AND_ASSIGN(auto value, Evaluate("ip('127.0.0.1').isLoopback()"));
  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.As<cel::BoolValue>()->NativeValue(), Eq(true));
}

TEST_F(NetworkExtTest, IsLoopbackIPv6) {
  ASSERT_OK_AND_ASSIGN(auto value, Evaluate("ip('::1').isLoopback()"));
  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.As<cel::BoolValue>()->NativeValue(), Eq(true));
}

TEST_F(NetworkExtTest, IsUnspecifiedIPv4) {
  ASSERT_OK_AND_ASSIGN(auto value, Evaluate("ip('0.0.0.0').isUnspecified()"));
  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.As<cel::BoolValue>()->NativeValue(), Eq(true));
}

TEST_F(NetworkExtTest, IsUnspecifiedIPv6) {
  ASSERT_OK_AND_ASSIGN(auto value, Evaluate("ip('::').isUnspecified()"));
  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.As<cel::BoolValue>()->NativeValue(), Eq(true));
}

TEST_F(NetworkExtTest, IsGlobalUnicast) {
  ASSERT_OK_AND_ASSIGN(auto value, Evaluate("ip('8.8.8.8').isGlobalUnicast()"));
  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.As<cel::BoolValue>()->NativeValue(), Eq(true));
}

TEST_F(NetworkExtTest, IsLinkLocalMulticast) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       Evaluate("ip('ff02::1').isLinkLocalMulticast()"));
  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.As<cel::BoolValue>()->NativeValue(), Eq(true));
}

// --- CIDR Accessors ---
TEST_F(NetworkExtTest, CIDRPrefixLength) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       Evaluate("cidr('192.168.0.0/24').prefixLength()"));
  ASSERT_TRUE(value.IsInt());
  EXPECT_THAT(value.As<cel::IntValue>()->NativeValue(), Eq(24));
}

TEST_F(NetworkExtTest, CIDRIPExtraction) {
  ASSERT_OK_AND_ASSIGN(
      auto value, Evaluate("cidr('192.168.0.0/24').ip() == ip('192.168.0.0')"));
  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.As<cel::BoolValue>()->NativeValue(), Eq(true));
}

TEST_F(NetworkExtTest, CIDRIPExtractionHostBitsSet) {
  ASSERT_OK_AND_ASSIGN(
      auto value, Evaluate("cidr('192.168.1.5/24').ip() == ip('192.168.1.5')"));
  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.As<cel::BoolValue>()->NativeValue(), Eq(true));
}

TEST_F(NetworkExtTest, CIDRMasked) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      Evaluate("cidr('192.168.1.5/24').masked() == cidr('192.168.1.0/24')"));
  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.As<cel::BoolValue>()->NativeValue(), Eq(true));
}

TEST_F(NetworkExtTest, CIDRMaskedIdentity) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      Evaluate("cidr('192.168.1.0/24').masked() == cidr('192.168.1.0/24')"));
  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.As<cel::BoolValue>()->NativeValue(), Eq(true));
}

// --- Containment (IP in CIDR) ---
TEST_F(NetworkExtTest, ContainsIPSimple) {
  ASSERT_OK_AND_ASSIGN(
      auto value, Evaluate("cidr('10.0.0.0/8').containsIP(ip('10.1.2.3'))"));
  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.As<cel::BoolValue>()->NativeValue(), Eq(true));
}

TEST_F(NetworkExtTest, ContainsIPStringOverload) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       Evaluate("cidr('10.0.0.0/8').containsIP('10.1.2.3')"));
  ASSERT_TRUE(value.IsBool());
  EXPECT_THAT(value.As<cel::BoolValue>()->NativeValue(), Eq(true));
}

// ... other Contains tests ...

// --- Runtime Errors ---
TEST_F(NetworkExtTest, ErrIPConstructorInvalid) {
  ASSERT_OK_AND_ASSIGN(auto value, Evaluate("ip('999.999.999.999')"));
  ASSERT_TRUE(value.IsError());
  EXPECT_THAT(
      value.As<cel::ErrorValue>()->ToStatus(),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("parse error")));
}

TEST_F(NetworkExtTest, ErrCIDRConstructorInvalid) {
  ASSERT_OK_AND_ASSIGN(auto value, Evaluate("cidr('1.2.3.4')"));
  ASSERT_TRUE(value.IsError());
  EXPECT_THAT(
      value.As<cel::ErrorValue>()->ToStatus(),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("parse error")));
}

TEST_F(NetworkExtTest, ErrCIDRConstructorInvalidMask) {
  ASSERT_OK_AND_ASSIGN(auto value, Evaluate("cidr('10.0.0.0/999')"));
  ASSERT_TRUE(value.IsError());
  EXPECT_THAT(
      value.As<cel::ErrorValue>()->ToStatus(),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("parse error")));
}

TEST_F(NetworkExtTest, ErrContainsIPStringInvalid) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       Evaluate("cidr('10.0.0.0/8').containsIP('not-an-ip')"));
  ASSERT_TRUE(value.IsError());
  EXPECT_THAT(value.As<cel::ErrorValue>()->ToStatus(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid or non-strict IP string")));
}

TEST_F(NetworkExtTest, ErrContainsCIDRStringInvalid) {
  ASSERT_OK_AND_ASSIGN(
      auto value, Evaluate("cidr('10.0.0.0/8').containsCIDR('not-a-cidr')"));
  ASSERT_TRUE(value.IsError());
  EXPECT_THAT(value.As<cel::ErrorValue>()->ToStatus(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid or non-strict CIDR string")));
}

}  // namespace
}  // namespace cel::extensions
