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
#include <string>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "base/ast.h"

namespace cel::ast::internal {

absl::StatusOr<Constant> ToNative(const google::api::expr::v1alpha1::Constant& constant) {
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
      return Constant(constant.bytes_value());
    case google::api::expr::v1alpha1::Constant::kDurationValue:
      return Constant(absl::Seconds(constant.duration_value().seconds()) +
                      absl::Nanoseconds(constant.duration_value().nanos()));
    case google::api::expr::v1alpha1::Constant::kTimestampValue:
      return Constant(
          absl::FromUnixSeconds(constant.timestamp_value().seconds()) +
          absl::Nanoseconds(constant.timestamp_value().nanos()));
    default:
      return absl::InvalidArgumentError(
          "Illegal type supplied for google::api::expr::v1alpha1::Constant.");
  }
}

Ident ToNative(const google::api::expr::v1alpha1::Expr::Ident& ident) {
  return Ident(ident.name());
}

absl::StatusOr<Select> ToNative(const google::api::expr::v1alpha1::Expr::Select& select) {
  auto native_operand = ToNative(select.operand());
  if (!native_operand.ok()) {
    return native_operand.status();
  }
  return Select(std::make_unique<Expr>(*std::move(native_operand)),
                select.field(), select.test_only());
}

absl::StatusOr<Call> ToNative(const google::api::expr::v1alpha1::Expr::Call& call) {
  std::vector<Expr> args;
  args.reserve(call.args_size());
  for (const auto& arg : call.args()) {
    auto native_arg = ToNative(arg);
    if (!native_arg.ok()) {
      return native_arg.status();
    }
    args.emplace_back(*(std::move(native_arg)));
  }
  auto native_target = ToNative(call.target());
  if (!native_target.ok()) {
    return native_target.status();
  }
  return Call(std::make_unique<Expr>(*(std::move(native_target))),
              call.function(), std::move(args));
}

absl::StatusOr<CreateList> ToNative(
    const google::api::expr::v1alpha1::Expr::CreateList& create_list) {
  CreateList ret_val;
  ret_val.mutable_elements().reserve(create_list.elements_size());
  for (const auto& elem : create_list.elements()) {
    auto native_elem = ToNative(elem);
    if (!native_elem.ok()) {
      return native_elem.status();
    }
    ret_val.mutable_elements().emplace_back(*std::move(native_elem));
  }
  return ret_val;
}

absl::StatusOr<CreateStruct::Entry> ToNative(
    const google::api::expr::v1alpha1::Expr::CreateStruct::Entry& entry) {
  auto key = [](const google::api::expr::v1alpha1::Expr::CreateStruct::Entry& entry)
      -> absl::StatusOr<CreateStruct::Entry::KeyKind> {
    switch (entry.key_kind_case()) {
      case google::api::expr::v1alpha1::Expr_CreateStruct_Entry::kFieldKey:
        return entry.field_key();
      case google::api::expr::v1alpha1::Expr_CreateStruct_Entry::kMapKey: {
        auto native_map_key = ToNative(entry.map_key());
        if (!native_map_key.ok()) {
          return native_map_key.status();
        }
        return std::make_unique<Expr>(*(std::move(native_map_key)));
      }
      default:
        return absl::InvalidArgumentError(
            "Illegal type provided for "
            "google::api::expr::v1alpha1::Expr::CreateStruct::Entry::key_kind.");
    }
  };
  auto native_key = key(entry);
  if (!native_key.ok()) {
    return native_key.status();
  }
  auto native_value = ToNative(entry.value());
  if (!native_value.ok()) {
    return native_value.status();
  }
  return CreateStruct::Entry(
      entry.id(), *std::move(native_key),
      std::make_unique<Expr>(*(std::move(native_value))));
}

absl::StatusOr<CreateStruct> ToNative(
    const google::api::expr::v1alpha1::Expr::CreateStruct& create_struct) {
  std::vector<CreateStruct::Entry> entries;
  entries.reserve(create_struct.entries_size());
  for (const auto& entry : create_struct.entries()) {
    auto native_entry = ToNative(entry);
    if (!native_entry.ok()) {
      return native_entry.status();
    }
    entries.emplace_back(*(std::move(native_entry)));
  }
  return CreateStruct(create_struct.message_name(), std::move(entries));
}

absl::StatusOr<Comprehension> ToNative(
    const google::api::expr::v1alpha1::Expr::Comprehension& comprehension) {
  Comprehension ret_val;
  ret_val.set_iter_var(comprehension.iter_var());
  if (comprehension.has_iter_range()) {
    auto native_iter_range = ToNative(comprehension.iter_range());
    if (!native_iter_range.ok()) {
      return native_iter_range.status();
    }
    ret_val.set_iter_range(
        std::make_unique<Expr>(*(std::move(native_iter_range))));
  }
  ret_val.set_accu_var(comprehension.accu_var());
  if (comprehension.has_accu_init()) {
    auto native_accu_init = ToNative(comprehension.accu_init());
    if (!native_accu_init.ok()) {
      return native_accu_init.status();
    }
    ret_val.set_accu_init(
        std::make_unique<Expr>(*(std::move(native_accu_init))));
  }
  if (comprehension.has_loop_condition()) {
    auto native_loop_condition = ToNative(comprehension.loop_condition());
    if (!native_loop_condition.ok()) {
      return native_loop_condition.status();
    }
    ret_val.set_loop_condition(
        std::make_unique<Expr>(*(std::move(native_loop_condition))));
  }
  if (comprehension.has_loop_step()) {
    auto native_loop_step = ToNative(comprehension.loop_step());
    if (!native_loop_step.ok()) {
      return native_loop_step.status();
    }
    ret_val.set_loop_step(
        std::make_unique<Expr>(*(std::move(native_loop_step))));
  }
  if (comprehension.has_result()) {
    auto native_result = ToNative(comprehension.result());
    if (!native_result.ok()) {
      return native_result.status();
    }
    ret_val.set_result(std::make_unique<Expr>(*(std::move(native_result))));
  }
  return ret_val;
}

absl::StatusOr<Expr> ToNative(const google::api::expr::v1alpha1::Expr& expr) {
  switch (expr.expr_kind_case()) {
    case google::api::expr::v1alpha1::Expr::kConstExpr: {
      auto native_const = ToNative(expr.const_expr());
      if (!native_const.ok()) {
        return native_const.status();
      }
      return Expr(expr.id(), *(std::move(native_const)));
    }
    case google::api::expr::v1alpha1::Expr::kIdentExpr:
      return Expr(expr.id(), ToNative(expr.ident_expr()));
    case google::api::expr::v1alpha1::Expr::kSelectExpr: {
      auto native_select = ToNative(expr.select_expr());
      if (!native_select.ok()) {
        return native_select.status();
      }
      return Expr(expr.id(), *(std::move(native_select)));
    }
    case google::api::expr::v1alpha1::Expr::kCallExpr: {
      auto native_call = ToNative(expr.call_expr());
      if (!native_call.ok()) {
        return native_call.status();
      }
      return Expr(expr.id(), *(std::move(native_call)));
    }
    case google::api::expr::v1alpha1::Expr::kListExpr: {
      auto native_list = ToNative(expr.list_expr());
      if (!native_list.ok()) {
        return native_list.status();
      }
      return Expr(expr.id(), *(std::move(native_list)));
    }
    case google::api::expr::v1alpha1::Expr::kStructExpr: {
      auto native_struct = ToNative(expr.struct_expr());
      if (!native_struct.ok()) {
        return native_struct.status();
      }
      return Expr(expr.id(), *(std::move(native_struct)));
    }
    case google::api::expr::v1alpha1::Expr::kComprehensionExpr: {
      auto native_comprehension = ToNative(expr.comprehension_expr());
      if (!native_comprehension.ok()) {
        return native_comprehension.status();
      }
      return Expr(expr.id(), *(std::move(native_comprehension)));
    }
    default:
      return absl::InvalidArgumentError(
          "Illegal type supplied for google::api::expr::v1alpha1::Expr::expr_kind.");
  }
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
  std::vector<std::string> overload_id;
  overload_id.reserve(reference.overload_id_size());
  for (const auto& elem : reference.overload_id()) {
    overload_id.emplace_back(elem);
  }
  auto native_value = ToNative(reference.value());
  if (!native_value.ok()) {
    return native_value.status();
  }
  return Reference(reference.name(), std::move(overload_id),
                   *(std::move(native_value)));
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
