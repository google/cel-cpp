// Copyright 2022 Google LLC
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

#include "base/ast_utility.h"

#include <cstdint>
#include <memory>
#include <stack>
#include <string>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "absl/types/variant.h"
#include "base/ast.h"
#include "internal/status_macros.h"

namespace cel::ast::internal {
namespace {

constexpr int kMaxIterations = 1'000'000;

struct ConversionStackEntry {
  // Not null.
  Expr* expr;
  // Not null.
  const ::google::api::expr::v1alpha1::Expr* proto_expr;
};

Ident ConvertIdent(const ::google::api::expr::v1alpha1::Expr::Ident& ident) {
  return Ident(ident.name());
}

absl::StatusOr<Select> ConvertSelect(
    const ::google::api::expr::v1alpha1::Expr::Select& select,
    std::stack<ConversionStackEntry>& stack) {
  Select value(std::make_unique<Expr>(), select.field(), select.test_only());
  stack.push({&value.mutable_operand(), &select.operand()});
  return value;
}

absl::StatusOr<Call> ConvertCall(const ::google::api::expr::v1alpha1::Expr::Call& call,
                                 std::stack<ConversionStackEntry>& stack) {
  Call ret_val;
  ret_val.set_function(call.function());
  ret_val.set_args(std::vector<Expr>(call.args_size()));
  for (int i = 0; i < ret_val.args().size(); i++) {
    stack.push({&ret_val.mutable_args()[i], &call.args(i)});
  }
  if (call.has_target()) {
    stack.push({&ret_val.mutable_target(), &call.target()});
  }
  return ret_val;
}

absl::StatusOr<CreateList> ConvertCreateList(
    const ::google::api::expr::v1alpha1::Expr::CreateList& create_list,
    std::stack<ConversionStackEntry>& stack) {
  CreateList ret_val;
  ret_val.set_elements(std::vector<Expr>(create_list.elements_size()));

  for (int i = 0; i < ret_val.elements().size(); i++) {
    stack.push({&ret_val.mutable_elements()[i], &create_list.elements(i)});
  }
  return ret_val;
}

absl::StatusOr<CreateStruct::Entry::KeyKind> ConvertCreateStructEntryKey(
    const ::google::api::expr::v1alpha1::Expr::CreateStruct::Entry& entry,
    std::stack<ConversionStackEntry>& stack) {
  switch (entry.key_kind_case()) {
    case google::api::expr::v1alpha1::Expr_CreateStruct_Entry::kFieldKey:
      return entry.field_key();
    case google::api::expr::v1alpha1::Expr_CreateStruct_Entry::kMapKey: {
      auto native_map_key = std::make_unique<Expr>();
      stack.push({native_map_key.get(), &entry.map_key()});
      return native_map_key;
    }
    default:
      return absl::InvalidArgumentError(
          "Illegal type provided for "
          "google::api::expr::v1alpha1::Expr::CreateStruct::Entry::key_kind.");
  }
}

absl::StatusOr<CreateStruct::Entry> ConvertCreateStructEntry(
    const ::google::api::expr::v1alpha1::Expr::CreateStruct::Entry& entry,
    std::stack<ConversionStackEntry>& stack) {
  CEL_ASSIGN_OR_RETURN(auto native_key,
                       ConvertCreateStructEntryKey(entry, stack));

  if (!entry.has_value()) {
    return absl::InvalidArgumentError(
        "google::api::expr::v1alpha1::Expr::CreateStruct::Entry missing value");
  }
  CreateStruct::Entry result(entry.id(), std::move(native_key),
                             std::make_unique<Expr>());
  stack.push({&result.mutable_value(), &entry.value()});

  return result;
}

absl::StatusOr<CreateStruct> ConvertCreateStruct(
    const ::google::api::expr::v1alpha1::Expr::CreateStruct& create_struct,
    std::stack<ConversionStackEntry>& stack) {
  std::vector<CreateStruct::Entry> entries;
  entries.reserve(create_struct.entries_size());
  for (const auto& entry : create_struct.entries()) {
    CEL_ASSIGN_OR_RETURN(auto native_entry,
                         ConvertCreateStructEntry(entry, stack));
    entries.push_back(std::move(native_entry));
  }
  return CreateStruct(create_struct.message_name(), std::move(entries));
}

absl::StatusOr<Comprehension> ConvertComprehension(
    const google::api::expr::v1alpha1::Expr::Comprehension& comprehension,
    std::stack<ConversionStackEntry>& stack) {
  Comprehension ret_val;
  // accu_var
  if (comprehension.accu_var().empty()) {
    return absl::InvalidArgumentError(
        "Invalid comprehension: 'accu_var' must not be empty");
  }
  ret_val.set_accu_var(comprehension.accu_var());
  // iter_var
  if (comprehension.iter_var().empty()) {
    return absl::InvalidArgumentError(
        "Invalid comprehension: 'iter_var' must not be empty");
  }
  ret_val.set_iter_var(comprehension.iter_var());

  // accu_init
  if (!comprehension.has_accu_init()) {
    return absl::InvalidArgumentError(
        "Invalid comprehension: 'accu_init' must be set");
  }
  stack.push({&ret_val.mutable_accu_init(), &comprehension.accu_init()});

  // iter_range optional
  if (comprehension.has_iter_range()) {
    stack.push({&ret_val.mutable_iter_range(), &comprehension.iter_range()});
  }

  // loop_condition
  if (!comprehension.has_loop_condition()) {
    return absl::InvalidArgumentError(
        "Invalid comprehension: 'loop_condition' must be set");
  }
  stack.push(
      {&ret_val.mutable_loop_condition(), &comprehension.loop_condition()});

  // loop_step
  if (!comprehension.has_loop_step()) {
    return absl::InvalidArgumentError(
        "Invalid comprehension: 'loop_step' must be set");
  }
  stack.push({&ret_val.mutable_loop_step(), &comprehension.loop_step()});

  // result
  if (!comprehension.has_result()) {
    return absl::InvalidArgumentError(
        "Invalid comprehension: 'result' must be set");
  }
  stack.push({&ret_val.mutable_result(), &comprehension.result()});

  return ret_val;
}

absl::StatusOr<Expr> ConvertExpr(const ::google::api::expr::v1alpha1::Expr& expr,
                                 std::stack<ConversionStackEntry>& stack) {
  switch (expr.expr_kind_case()) {
    case google::api::expr::v1alpha1::Expr::kConstExpr: {
      CEL_ASSIGN_OR_RETURN(auto native_const,
                           ConvertConstant(expr.const_expr()));
      return Expr(expr.id(), std::move(native_const));
    }
    case google::api::expr::v1alpha1::Expr::kIdentExpr:
      return Expr(expr.id(), ConvertIdent(expr.ident_expr()));
    case google::api::expr::v1alpha1::Expr::kSelectExpr: {
      CEL_ASSIGN_OR_RETURN(auto native_select,
                           ConvertSelect(expr.select_expr(), stack));
      return Expr(expr.id(), std::move(native_select));
    }
    case google::api::expr::v1alpha1::Expr::kCallExpr: {
      CEL_ASSIGN_OR_RETURN(auto native_call,
                           ConvertCall(expr.call_expr(), stack));

      return Expr(expr.id(), std::move(native_call));
    }
    case google::api::expr::v1alpha1::Expr::kListExpr: {
      CEL_ASSIGN_OR_RETURN(auto native_list,
                           ConvertCreateList(expr.list_expr(), stack));

      return Expr(expr.id(), std::move(native_list));
    }
    case google::api::expr::v1alpha1::Expr::kStructExpr: {
      CEL_ASSIGN_OR_RETURN(auto native_struct,
                           ConvertCreateStruct(expr.struct_expr(), stack));
      return Expr(expr.id(), std::move(native_struct));
    }
    case google::api::expr::v1alpha1::Expr::kComprehensionExpr: {
      CEL_ASSIGN_OR_RETURN(
          auto native_comprehension,
          ConvertComprehension(expr.comprehension_expr(), stack));
      return Expr(expr.id(), std::move(native_comprehension));
    }
    default:
      // kind unset
      return Expr(expr.id(), absl::monostate());
  }
}

absl::StatusOr<Expr> ToNativeExprImpl(
    const ::google::api::expr::v1alpha1::Expr& proto_expr) {
  std::stack<ConversionStackEntry> conversion_stack;
  int iterations = 0;
  Expr root;
  conversion_stack.push({&root, &proto_expr});
  while (!conversion_stack.empty()) {
    ConversionStackEntry entry = conversion_stack.top();
    conversion_stack.pop();
    CEL_ASSIGN_OR_RETURN(*entry.expr,
                         ConvertExpr(*entry.proto_expr, conversion_stack));
    ++iterations;
    if (iterations > kMaxIterations) {
      return absl::InternalError(
          "max iterations exceeded in proto to native ast conversion.");
    }
  }
  return root;
}

}  // namespace

absl::StatusOr<Constant> ConvertConstant(
    const google::api::expr::v1alpha1::Constant& constant) {
  switch (constant.constant_kind_case()) {
    case google::api::expr::v1alpha1::Constant::kNullValue:
      return Constant(NullValue::kNullValue);
    case google::api::expr::v1alpha1::Constant::kBoolValue:
      return Constant(constant.bool_value());
    case google::api::expr::v1alpha1::Constant::kInt64Value:
      return Constant(constant.int64_value());
    case google::api::expr::v1alpha1::Constant::kUint64Value:
      return Constant(constant.uint64_value());
    case google::api::expr::v1alpha1::Constant::kDoubleValue:
      return Constant(constant.double_value());
    case google::api::expr::v1alpha1::Constant::kStringValue:
      return Constant(constant.string_value());
    case google::api::expr::v1alpha1::Constant::kBytesValue:
      return Constant(Bytes{constant.bytes_value()});
    case google::api::expr::v1alpha1::Constant::kDurationValue:
      return Constant(absl::Seconds(constant.duration_value().seconds()) +
                      absl::Nanoseconds(constant.duration_value().nanos()));
    case google::api::expr::v1alpha1::Constant::kTimestampValue:
      return Constant(
          absl::FromUnixSeconds(constant.timestamp_value().seconds()) +
          absl::Nanoseconds(constant.timestamp_value().nanos()));
    default:
      return absl::InvalidArgumentError("Unsupported constant type");
  }
}

absl::StatusOr<Expr> ToNative(const google::api::expr::v1alpha1::Expr& expr) {
  return ToNativeExprImpl(expr);
}

absl::StatusOr<SourceInfo> ToNative(
    const google::api::expr::v1alpha1::SourceInfo& source_info) {
  absl::flat_hash_map<int64_t, Expr> macro_calls;
  for (const auto& pair : source_info.macro_calls()) {
    auto native_expr = ToNative(pair.second);
    if (!native_expr.ok()) {
      return native_expr.status();
    }
    macro_calls.emplace(pair.first, *(std::move(native_expr)));
  }
  return SourceInfo(
      source_info.syntax_version(), source_info.location(),
      std::vector<int32_t>(source_info.line_offsets().begin(),
                           source_info.line_offsets().end()),
      absl::flat_hash_map<int64_t, int32_t>(source_info.positions().begin(),
                                            source_info.positions().end()),
      std::move(macro_calls));
}

absl::StatusOr<ParsedExpr> ToNative(
    const google::api::expr::v1alpha1::ParsedExpr& parsed_expr) {
  auto native_expr = ToNative(parsed_expr.expr());
  if (!native_expr.ok()) {
    return native_expr.status();
  }
  auto native_source_info = ToNative(parsed_expr.source_info());
  if (!native_source_info.ok()) {
    return native_source_info.status();
  }
  return ParsedExpr(*(std::move(native_expr)),
                    *(std::move(native_source_info)));
}

absl::StatusOr<PrimitiveType> ToNative(
    google::api::expr::v1alpha1::Type::PrimitiveType primitive_type) {
  switch (primitive_type) {
    case google::api::expr::v1alpha1::Type::PRIMITIVE_TYPE_UNSPECIFIED:
      return PrimitiveType::kPrimitiveTypeUnspecified;
    case google::api::expr::v1alpha1::Type::BOOL:
      return PrimitiveType::kBool;
    case google::api::expr::v1alpha1::Type::INT64:
      return PrimitiveType::kInt64;
    case google::api::expr::v1alpha1::Type::UINT64:
      return PrimitiveType::kUint64;
    case google::api::expr::v1alpha1::Type::DOUBLE:
      return PrimitiveType::kDouble;
    case google::api::expr::v1alpha1::Type::STRING:
      return PrimitiveType::kString;
    case google::api::expr::v1alpha1::Type::BYTES:
      return PrimitiveType::kBytes;
    default:
      return absl::InvalidArgumentError(
          "Illegal type specified for "
          "google::api::expr::v1alpha1::Type::PrimitiveType.");
  }
}

absl::StatusOr<WellKnownType> ToNative(
    google::api::expr::v1alpha1::Type::WellKnownType well_known_type) {
  switch (well_known_type) {
    case google::api::expr::v1alpha1::Type::WELL_KNOWN_TYPE_UNSPECIFIED:
      return WellKnownType::kWellKnownTypeUnspecified;
    case google::api::expr::v1alpha1::Type::ANY:
      return WellKnownType::kAny;
    case google::api::expr::v1alpha1::Type::TIMESTAMP:
      return WellKnownType::kTimestamp;
    case google::api::expr::v1alpha1::Type::DURATION:
      return WellKnownType::kDuration;
    default:
      return absl::InvalidArgumentError(
          "Illegal type specified for "
          "google::api::expr::v1alpha1::Type::WellKnownType.");
  }
}

absl::StatusOr<ListType> ToNative(
    const google::api::expr::v1alpha1::Type::ListType& list_type) {
  auto native_elem_type = ToNative(list_type.elem_type());
  if (!native_elem_type.ok()) {
    return native_elem_type.status();
  }
  return ListType(std::make_unique<Type>(*(std::move(native_elem_type))));
}

absl::StatusOr<MapType> ToNative(
    const google::api::expr::v1alpha1::Type::MapType& map_type) {
  auto native_key_type = ToNative(map_type.key_type());
  if (!native_key_type.ok()) {
    return native_key_type.status();
  }
  auto native_value_type = ToNative(map_type.value_type());
  if (!native_value_type.ok()) {
    return native_value_type.status();
  }
  return MapType(std::make_unique<Type>(*(std::move(native_key_type))),
                 std::make_unique<Type>(*(std::move(native_value_type))));
}

absl::StatusOr<FunctionType> ToNative(
    const google::api::expr::v1alpha1::Type::FunctionType& function_type) {
  std::vector<Type> arg_types;
  arg_types.reserve(function_type.arg_types_size());
  for (const auto& arg_type : function_type.arg_types()) {
    auto native_arg = ToNative(arg_type);
    if (!native_arg.ok()) {
      return native_arg.status();
    }
    arg_types.emplace_back(*(std::move(native_arg)));
  }
  auto native_result = ToNative(function_type.result_type());
  if (!native_result.ok()) {
    return native_result.status();
  }
  return FunctionType(std::make_unique<Type>(*(std::move(native_result))),
                      std::move(arg_types));
}

absl::StatusOr<AbstractType> ToNative(
    const google::api::expr::v1alpha1::Type::AbstractType& abstract_type) {
  std::vector<Type> parameter_types;
  for (const auto& parameter_type : abstract_type.parameter_types()) {
    auto native_parameter_type = ToNative(parameter_type);
    if (!native_parameter_type.ok()) {
      return native_parameter_type.status();
    }
    parameter_types.emplace_back(*(std::move(native_parameter_type)));
  }
  return AbstractType(abstract_type.name(), std::move(parameter_types));
}

absl::StatusOr<Type> ToNative(const google::api::expr::v1alpha1::Type& type) {
  switch (type.type_kind_case()) {
    case google::api::expr::v1alpha1::Type::kDyn:
      return Type(DynamicType());
    case google::api::expr::v1alpha1::Type::kNull:
      return Type(NullValue::kNullValue);
    case google::api::expr::v1alpha1::Type::kPrimitive: {
      auto native_primitive = ToNative(type.primitive());
      if (!native_primitive.ok()) {
        return native_primitive.status();
      }
      return Type(*(std::move(native_primitive)));
    }
    case google::api::expr::v1alpha1::Type::kWrapper: {
      auto native_wrapper = ToNative(type.wrapper());
      if (!native_wrapper.ok()) {
        return native_wrapper.status();
      }
      return Type(PrimitiveTypeWrapper(*(std::move(native_wrapper))));
    }
    case google::api::expr::v1alpha1::Type::kWellKnown: {
      auto native_well_known = ToNative(type.well_known());
      if (!native_well_known.ok()) {
        return native_well_known.status();
      }
      return Type(*std::move(native_well_known));
    }
    case google::api::expr::v1alpha1::Type::kListType: {
      auto native_list_type = ToNative(type.list_type());
      if (!native_list_type.ok()) {
        return native_list_type.status();
      }
      return Type(*(std::move(native_list_type)));
    }
    case google::api::expr::v1alpha1::Type::kMapType: {
      auto native_map_type = ToNative(type.map_type());
      if (!native_map_type.ok()) {
        return native_map_type.status();
      }
      return Type(*(std::move(native_map_type)));
    }
    case google::api::expr::v1alpha1::Type::kFunction: {
      auto native_function = ToNative(type.function());
      if (!native_function.ok()) {
        return native_function.status();
      }
      return Type(*(std::move(native_function)));
    }
    case google::api::expr::v1alpha1::Type::kMessageType:
      return Type(MessageType(type.message_type()));
    case google::api::expr::v1alpha1::Type::kTypeParam:
      return Type(ParamType(type.type_param()));
    case google::api::expr::v1alpha1::Type::kType: {
      auto native_type = ToNative(type.type());
      if (!native_type.ok()) {
        return native_type.status();
      }
      return Type(std::make_unique<Type>(*std::move(native_type)));
    }
    case google::api::expr::v1alpha1::Type::kError:
      return Type(ErrorType::kErrorTypeValue);
    case google::api::expr::v1alpha1::Type::kAbstractType: {
      auto native_abstract = ToNative(type.abstract_type());
      if (!native_abstract.ok()) {
        return native_abstract.status();
      }
      return Type(*(std::move(native_abstract)));
    }
    default:
      return absl::InvalidArgumentError(
          "Illegal type specified for google::api::expr::v1alpha1::Type.");
  }
}

absl::StatusOr<Reference> ToNative(
    const google::api::expr::v1alpha1::Reference& reference) {
  Reference ret_val;
  ret_val.set_name(reference.name());
  ret_val.mutable_overload_id().reserve(reference.overload_id_size());
  for (const auto& elem : reference.overload_id()) {
    ret_val.mutable_overload_id().emplace_back(elem);
  }
  if (reference.has_value()) {
    auto native_value = ConvertConstant(reference.value());
    if (!native_value.ok()) {
      return native_value.status();
    }
    ret_val.set_value(*(std::move(native_value)));
  }
  return ret_val;
}

absl::StatusOr<CheckedExpr> ToNative(
    const google::api::expr::v1alpha1::CheckedExpr& checked_expr) {
  CheckedExpr ret_val;
  for (const auto& pair : checked_expr.reference_map()) {
    auto native_reference = ToNative(pair.second);
    if (!native_reference.ok()) {
      return native_reference.status();
    }
    ret_val.mutable_reference_map().emplace(pair.first,
                                            *(std::move(native_reference)));
  }
  for (const auto& pair : checked_expr.type_map()) {
    auto native_type = ToNative(pair.second);
    if (!native_type.ok()) {
      return native_type.status();
    }
    ret_val.mutable_type_map().emplace(pair.first, *(std::move(native_type)));
  }
  auto native_source_info = ToNative(checked_expr.source_info());
  if (!native_source_info.ok()) {
    return native_source_info.status();
  }
  ret_val.set_source_info(*(std::move(native_source_info)));
  ret_val.set_expr_version(checked_expr.expr_version());
  auto native_checked_expr = ToNative(checked_expr.expr());
  if (!native_checked_expr.ok()) {
    return native_checked_expr.status();
  }
  ret_val.set_expr(*(std::move(native_checked_expr)));
  return ret_val;
}

}  // namespace cel::ast::internal
