// Copyright 2024 Google LLC
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

#include "conformance/service.h"

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "google/api/expr/conformance/v1alpha1/conformance_service.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/eval.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/api/expr/v1alpha1/value.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/empty.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/rpc/code.pb.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/memory.h"
#include "common/source.h"
#include "common/value.h"
#include "conformance/value_conversion.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/transform_utility.h"
#include "extensions/bindings_ext.h"
#include "extensions/encoders.h"
#include "extensions/math_ext.h"
#include "extensions/math_ext_macros.h"
#include "extensions/proto_ext.h"
#include "extensions/protobuf/enum_adapter.h"
#include "extensions/protobuf/memory_manager.h"
#include "extensions/protobuf/runtime_adapter.h"
#include "extensions/protobuf/type_reflector.h"
#include "extensions/strings.h"
#include "internal/status_macros.h"
#include "parser/macro_registry.h"
#include "parser/options.h"
#include "parser/parser.h"
#include "parser/standard_macros.h"
#include "runtime/activation.h"
#include "runtime/constant_folding.h"
#include "runtime/managed_value_factory.h"
#include "runtime/optional_types.h"
#include "runtime/reference_resolver.h"
#include "runtime/runtime.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "proto/test/v1/proto2/test_all_types.pb.h"
#include "proto/test/v1/proto2/test_all_types_extensions.pb.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/message.h"


using ::cel::CreateStandardRuntimeBuilder;
using ::cel::Runtime;
using ::cel::RuntimeOptions;
using ::cel::conformance_internal::FromConformanceValue;
using ::cel::conformance_internal::ToConformanceValue;
using ::cel::extensions::ProtobufRuntimeAdapter;
using ::cel::extensions::ProtoMemoryManagerRef;
using ::cel::extensions::RegisterProtobufEnum;

using ::google::protobuf::Arena;

namespace google::api::expr::runtime {

namespace {

google::rpc::Code ToGrpcCode(absl::StatusCode code) {
  return static_cast<google::rpc::Code>(code);
}

using ConformanceServiceInterface =
    ::cel_conformance::ConformanceServiceInterface;

// Return a normalized raw expr for evaluation.
google::api::expr::v1alpha1::Expr ExtractExpr(
    const conformance::v1alpha1::EvalRequest& request) {
  const v1alpha1::Expr* expr = nullptr;

  // For now, discard type-check information if any.
  if (request.has_parsed_expr()) {
    expr = &request.parsed_expr().expr();
  } else if (request.has_checked_expr()) {
    expr = &request.checked_expr().expr();
  }
  google::api::expr::v1alpha1::Expr out;
  (out).MergeFrom(*expr);
  return out;
}

absl::Status LegacyParse(const conformance::v1alpha1::ParseRequest& request,
                         conformance::v1alpha1::ParseResponse& response,
                         bool enable_optional_syntax) {
  if (request.cel_source().empty()) {
    return absl::InvalidArgumentError("no source code");
  }
  cel::ParserOptions options;
  options.enable_optional_syntax = enable_optional_syntax;
  cel::MacroRegistry macros;
  CEL_RETURN_IF_ERROR(cel::RegisterStandardMacros(macros, options));
  CEL_RETURN_IF_ERROR(cel::extensions::RegisterBindingsMacros(macros, options));
  CEL_RETURN_IF_ERROR(cel::extensions::RegisterMathMacros(macros, options));
  CEL_RETURN_IF_ERROR(cel::extensions::RegisterProtoMacros(macros, options));
  CEL_ASSIGN_OR_RETURN(auto source, cel::NewSource(request.cel_source(),
                                                   request.source_location()));
  CEL_ASSIGN_OR_RETURN(auto parsed_expr,
                       parser::Parse(*source, macros, options));
  (*response.mutable_parsed_expr()).MergeFrom(parsed_expr);
  return absl::OkStatus();
}

class LegacyConformanceServiceImpl : public ConformanceServiceInterface {
 public:
  static absl::StatusOr<std::unique_ptr<LegacyConformanceServiceImpl>> Create(
      bool optimize, bool recursive) {
    static auto* constant_arena = new Arena();

    google::protobuf::LinkMessageReflection<
        google::api::expr::test::v1::proto3::TestAllTypes>();
    google::protobuf::LinkMessageReflection<
        google::api::expr::test::v1::proto2::TestAllTypes>();
    google::protobuf::LinkMessageReflection<
        google::api::expr::test::v1::proto3::NestedTestAllTypes>();
    google::protobuf::LinkMessageReflection<
        google::api::expr::test::v1::proto2::NestedTestAllTypes>();
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::int32_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::nested_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::test_all_types_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::nested_enum_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::repeated_test_all_types);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::Proto2ExtensionScopedMessage::
            int64_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::Proto2ExtensionScopedMessage::
            message_scoped_nested_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::Proto2ExtensionScopedMessage::
            nested_enum_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::Proto2ExtensionScopedMessage::
            message_scoped_repeated_test_all_types);

    InterpreterOptions options;
    options.enable_qualified_type_identifiers = true;
    options.enable_timestamp_duration_overflow_errors = true;
    options.enable_heterogeneous_equality = true;
    options.enable_empty_wrapper_null_unboxing = true;
    options.enable_qualified_identifier_rewrites = true;

    if (optimize) {
      std::cerr << "Enabling optimizations" << std::endl;
      options.constant_folding = true;
      options.constant_arena = constant_arena;
    }

    if (recursive) {
      options.max_recursion_depth = 48;
    }

    std::unique_ptr<CelExpressionBuilder> builder =
        CreateCelExpressionBuilder(options);
    auto type_registry = builder->GetTypeRegistry();
    type_registry->Register(
        google::api::expr::test::v1::proto2::GlobalEnum_descriptor());
    type_registry->Register(
        google::api::expr::test::v1::proto3::GlobalEnum_descriptor());
    type_registry->Register(google::api::expr::test::v1::proto2::TestAllTypes::
                                NestedEnum_descriptor());
    type_registry->Register(google::api::expr::test::v1::proto3::TestAllTypes::
                                NestedEnum_descriptor());
    CEL_RETURN_IF_ERROR(
        RegisterBuiltinFunctions(builder->GetRegistry(), options));
    CEL_RETURN_IF_ERROR(cel::extensions::RegisterEncodersFunctions(
        builder->GetRegistry(), options));
    CEL_RETURN_IF_ERROR(cel::extensions::RegisterStringsFunctions(
        builder->GetRegistry(), options));
    CEL_RETURN_IF_ERROR(cel::extensions::RegisterMathExtensionFunctions(
        builder->GetRegistry(), options));

    return absl::WrapUnique(
        new LegacyConformanceServiceImpl(std::move(builder)));
  }

  void Parse(const conformance::v1alpha1::ParseRequest& request,
             conformance::v1alpha1::ParseResponse& response) override {
    auto status =
        LegacyParse(request, response, /*enable_optional_syntax=*/false);
    if (!status.ok()) {
      auto* issue = response.add_issues();
      issue->set_code(ToGrpcCode(status.code()));
      issue->set_message(status.message());
    }
  }

  void Check(const conformance::v1alpha1::CheckRequest& request,
             conformance::v1alpha1::CheckResponse& response) override {
    auto issue = response.add_issues();
    issue->set_message("Check is not supported");
    issue->set_code(google::rpc::Code::UNIMPLEMENTED);
  }

  absl::Status Eval(const conformance::v1alpha1::EvalRequest& request,
                    conformance::v1alpha1::EvalResponse& response) override {
    Arena arena;
    google::api::expr::v1alpha1::SourceInfo source_info;
    google::api::expr::v1alpha1::Expr expr = ExtractExpr(request);
    builder_->set_container(request.container());
    auto cel_expression_status =
        builder_->CreateExpression(&expr, &source_info);

    if (!cel_expression_status.ok()) {
      return absl::InternalError(cel_expression_status.status().ToString());
    }

    auto cel_expression = std::move(cel_expression_status.value());
    Activation activation;

    for (const auto& pair : request.bindings()) {
      auto* import_value = Arena::Create<google::api::expr::v1alpha1::Value>(&arena);
      (*import_value).MergeFrom(pair.second.value());
      auto import_status = ValueToCelValue(*import_value, &arena);
      if (!import_status.ok()) {
        return absl::InternalError(import_status.status().ToString());
      }
      activation.InsertValue(pair.first, import_status.value());
    }

    auto eval_status = cel_expression->Evaluate(activation, &arena);
    if (!eval_status.ok()) {
      *response.mutable_result()
           ->mutable_error()
           ->add_errors()
           ->mutable_message() = eval_status.status().ToString();
      return absl::OkStatus();
    }

    CelValue result = eval_status.value();
    if (result.IsError()) {
      *response.mutable_result()
           ->mutable_error()
           ->add_errors()
           ->mutable_message() = std::string(result.ErrorOrDie()->message());
    } else {
      google::api::expr::v1alpha1::Value export_value;
      auto export_status = CelValueToValue(result, &export_value);
      if (!export_status.ok()) {
        return absl::InternalError(export_status.ToString());
      }
      auto* result_value = response.mutable_result()->mutable_value();
      (*result_value).MergeFrom(export_value);
    }
    return absl::OkStatus();
  }

 private:
  explicit LegacyConformanceServiceImpl(
      std::unique_ptr<CelExpressionBuilder> builder)
      : builder_(std::move(builder)) {}

  std::unique_ptr<CelExpressionBuilder> builder_;
};

class ModernConformanceServiceImpl : public ConformanceServiceInterface {
 public:
  static absl::StatusOr<std::unique_ptr<ModernConformanceServiceImpl>> Create(
      bool optimize, bool use_arena, bool recursive) {
    google::protobuf::LinkMessageReflection<
        google::api::expr::test::v1::proto3::TestAllTypes>();
    google::protobuf::LinkMessageReflection<
        google::api::expr::test::v1::proto2::TestAllTypes>();
    google::protobuf::LinkMessageReflection<
        google::api::expr::test::v1::proto3::NestedTestAllTypes>();
    google::protobuf::LinkMessageReflection<
        google::api::expr::test::v1::proto2::NestedTestAllTypes>();
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::int32_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::nested_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::test_all_types_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::nested_enum_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::repeated_test_all_types);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::Proto2ExtensionScopedMessage::
            int64_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::Proto2ExtensionScopedMessage::
            message_scoped_nested_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::Proto2ExtensionScopedMessage::
            nested_enum_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::Proto2ExtensionScopedMessage::
            message_scoped_repeated_test_all_types);

    RuntimeOptions options;
    options.enable_qualified_type_identifiers = true;
    options.enable_timestamp_duration_overflow_errors = true;
    options.enable_heterogeneous_equality = true;
    options.enable_empty_wrapper_null_unboxing = true;
    if (recursive) {
      options.max_recursion_depth = 48;
    }

    return absl::WrapUnique(
        new ModernConformanceServiceImpl(options, use_arena, optimize));
  }

  absl::StatusOr<std::unique_ptr<const cel::Runtime>> Setup(
      absl::string_view container) {
    RuntimeOptions options(options_);
    options.container = std::string(container);
    CEL_ASSIGN_OR_RETURN(auto builder, CreateStandardRuntimeBuilder(options));

    if (enable_optimizations_) {
      CEL_RETURN_IF_ERROR(cel::extensions::EnableConstantFolding(
          builder, constant_memory_manager_));
    }
    CEL_RETURN_IF_ERROR(cel::EnableReferenceResolver(
        builder, cel::ReferenceResolverEnabled::kAlways));

    auto& type_registry = builder.type_registry();
    // Use linked pbs in the
    type_registry.AddTypeProvider(
        std::make_unique<cel::extensions::ProtoTypeReflector>());
    CEL_RETURN_IF_ERROR(RegisterProtobufEnum(
        type_registry,
        google::api::expr::test::v1::proto2::GlobalEnum_descriptor()));
    CEL_RETURN_IF_ERROR(RegisterProtobufEnum(
        type_registry,
        google::api::expr::test::v1::proto3::GlobalEnum_descriptor()));
    CEL_RETURN_IF_ERROR(RegisterProtobufEnum(
        type_registry, google::api::expr::test::v1::proto2::TestAllTypes::
                           NestedEnum_descriptor()));
    CEL_RETURN_IF_ERROR(RegisterProtobufEnum(
        type_registry, google::api::expr::test::v1::proto3::TestAllTypes::
                           NestedEnum_descriptor()));

    CEL_RETURN_IF_ERROR(cel::extensions::EnableOptionalTypes(builder));
    CEL_RETURN_IF_ERROR(cel::extensions::RegisterEncodersFunctions(
        builder.function_registry(), options));
    CEL_RETURN_IF_ERROR(cel::extensions::RegisterStringsFunctions(
        builder.function_registry(), options));
    CEL_RETURN_IF_ERROR(cel::extensions::RegisterMathExtensionFunctions(
        builder.function_registry(), options));

    return std::move(builder).Build();
  }

  void Parse(const conformance::v1alpha1::ParseRequest& request,
             conformance::v1alpha1::ParseResponse& response) override {
    auto status =
        LegacyParse(request, response, /*enable_optional_syntax=*/true);
    if (!status.ok()) {
      auto* issue = response.add_issues();
      issue->set_code(ToGrpcCode(status.code()));
      issue->set_message(status.message());
    }
  }

  void Check(const conformance::v1alpha1::CheckRequest& request,
             conformance::v1alpha1::CheckResponse& response) override {
    auto issue = response.add_issues();
    issue->set_message("Check is not supported");
    issue->set_code(google::rpc::Code::UNIMPLEMENTED);
  }

  absl::Status Eval(const conformance::v1alpha1::EvalRequest& request,
                    conformance::v1alpha1::EvalResponse& response) override {
    google::api::expr::v1alpha1::Expr expr = ExtractExpr(request);
    google::protobuf::Arena arena;
    auto proto_memory_manager = ProtoMemoryManagerRef(&arena);
    cel::MemoryManagerRef memory_manager =
        (use_arena_ ? proto_memory_manager
                    : cel::MemoryManagerRef::ReferenceCounting());

    auto runtime_status = Setup(request.container());
    if (!runtime_status.ok()) {
      return absl::InternalError(runtime_status.status().ToString(
          absl::StatusToStringMode::kWithEverything));
    }
    std::unique_ptr<const cel::Runtime> runtime =
        std::move(runtime_status).value();

    auto program_status = ProtobufRuntimeAdapter::CreateProgram(*runtime, expr);
    if (!program_status.ok()) {
      return absl::InternalError(program_status.status().ToString(
          absl::StatusToStringMode::kWithEverything));
    }
    std::unique_ptr<cel::TraceableProgram> program =
        std::move(program_status).value();
    cel::ManagedValueFactory value_factory(program->GetTypeProvider(),
                                           memory_manager);
    cel::Activation activation;

    for (const auto& pair : request.bindings()) {
      google::api::expr::v1alpha1::Value import_value;
      (import_value).MergeFrom(pair.second.value());
      auto import_status =
          FromConformanceValue(value_factory.get(), import_value);
      if (!import_status.ok()) {
        return absl::InternalError(import_status.status().ToString());
      }

      activation.InsertOrAssignValue(pair.first,
                                     std::move(import_status).value());
    }

    auto eval_status = program->Evaluate(activation, value_factory.get());
    if (!eval_status.ok()) {
      *response.mutable_result()
           ->mutable_error()
           ->add_errors()
           ->mutable_message() = eval_status.status().ToString(
          absl::StatusToStringMode::kWithEverything);
      return absl::OkStatus();
    }

    cel::Value result = eval_status.value();
    if (result->Is<cel::ErrorValue>()) {
      const absl::Status& error = result->As<cel::ErrorValue>().NativeValue();
      *response.mutable_result()
           ->mutable_error()
           ->add_errors()
           ->mutable_message() = std::string(
          error.ToString(absl::StatusToStringMode::kWithEverything));
    } else {
      auto export_status = ToConformanceValue(value_factory.get(), result);
      if (!export_status.ok()) {
        return absl::InternalError(export_status.status().ToString(
            absl::StatusToStringMode::kWithEverything));
      }
      auto* result_value = response.mutable_result()->mutable_value();
      (*result_value).MergeFrom(*export_status);
    }
    return absl::OkStatus();
  }

 private:
  explicit ModernConformanceServiceImpl(const RuntimeOptions& options,
                                        bool use_arena,
                                        bool enable_optimizations)
      : options_(options),
        use_arena_(use_arena),
        enable_optimizations_(enable_optimizations),
        constant_memory_manager_(
            use_arena_ ? ProtoMemoryManagerRef(&constant_arena_)
                       : cel::MemoryManagerRef::ReferenceCounting()) {}

  RuntimeOptions options_;
  bool use_arena_;
  bool enable_optimizations_;
  Arena constant_arena_;
  cel::MemoryManagerRef constant_memory_manager_;
};

}  // namespace

}  // namespace google::api::expr::runtime

namespace cel_conformance {

absl::StatusOr<std::unique_ptr<ConformanceServiceInterface>>
NewConformanceService(const ConformanceServiceOptions& options) {
  if (options.modern) {
    return google::api::expr::runtime::ModernConformanceServiceImpl::Create(
        options.optimize, options.arena, options.recursive);
  } else {
    return google::api::expr::runtime::LegacyConformanceServiceImpl::Create(
        options.optimize, options.recursive);
  }
}

}  // namespace cel_conformance
