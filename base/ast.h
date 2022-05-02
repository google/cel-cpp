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

#ifndef THIRD_PARTY_CEL_CPP_BASE_AST_H_
#define THIRD_PARTY_CEL_CPP_BASE_AST_H_

#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/base/macros.h"
#include "absl/container/flat_hash_map.h"
#include "absl/time/time.h"
#include "absl/types/variant.h"
namespace cel::ast::internal {

enum class NullValue { kNullValue = 0 };

// Represents a primitive literal.
//
// This is similar as the primitives supported in the well-known type
// `google.protobuf.Value`, but richer so it can represent CEL's full range of
// primitives.
//
// Lists and structs are not included as constants as these aggregate types may
// contain [Expr][] elements which require evaluation and are thus not constant.
//
// Examples of constants include: `"hello"`, `b'bytes'`, `1u`, `4.2`, `-2`,
// `true`, `null`.
//
// (--
// TODO(issues/5): Extend or replace the constant with a canonical Value
// message that can hold any constant object representation supplied or
// produced at evaluation time.
// --)
using Constant = absl::variant<NullValue, bool, int64_t, uint64_t, double,
                               std::string, absl::Duration, absl::Time>;

class Expr;

// An identifier expression. e.g. `request`.
class Ident {
 public:
  explicit Ident(std::string name) : name_(std::move(name)) {}

  void set_name(std::string name) { name_ = std::move(name); }

  const std::string& name() const { return name_; }

 private:
  // Required. Holds a single, unqualified identifier, possibly preceded by a
  // '.'.
  //
  // Qualified names are represented by the [Expr.Select][] expression.
  std::string name_;
};

// A field selection expression. e.g. `request.auth`.
class Select {
 public:
  Select(std::unique_ptr<Expr> operand, std::string field,
         bool test_only = false)
      : operand_(std::move(operand)),
        field_(std::move(field)),
        test_only_(test_only) {}

  void set_operand(std::unique_ptr<Expr> operand) {
    operand_ = std::move(operand);
  }

  void set_field(std::string field) { field_ = std::move(field); }

  void set_test_only(bool test_only) { test_only_ = test_only; }

  const Expr* operand() const { return operand_.get(); }

  Expr& mutable_operand() {
    ABSL_ASSERT(operand_ != nullptr);
    return *operand_;
  }

  const std::string& field() const { return field_; }

  bool test_only() const { return test_only_; }

 private:
  // Required. The target of the selection expression.
  //
  // For example, in the select expression `request.auth`, the `request`
  // portion of the expression is the `operand`.
  std::unique_ptr<Expr> operand_;
  // Required. The name of the field to select.
  //
  // For example, in the select expression `request.auth`, the `auth` portion
  // of the expression would be the `field`.
  std::string field_;
  // Whether the select is to be interpreted as a field presence test.
  //
  // This results from the macro `has(request.auth)`.
  bool test_only_;
};

// A call expression, including calls to predefined functions and operators.
//
// For example, `value == 10`, `size(map_value)`.
// (-- TODO(issues/5): Convert built-in globals to instance methods --)
class Call {
 public:
  Call(std::unique_ptr<Expr> target, std::string function,
       std::vector<Expr> args)
      : target_(std::move(target)),
        function_(std::move(function)),
        args_(std::move(args)) {}

  void set_target(std::unique_ptr<Expr> target) { target_ = std::move(target); }

  void set_function(std::string function) { function_ = std::move(function); }

  void set_args(std::vector<Expr> args) { args_ = std::move(args); }

  const Expr* target() const { return target_.get(); }

  Expr& mutable_target() {
    ABSL_ASSERT(target_ != nullptr);
    return *target_;
  }

  const std::string& function() const { return function_; }

  const std::vector<Expr>& args() const { return args_; }

  std::vector<Expr>& mutable_args() { return args_; }

 private:
  // The target of an method call-style expression. For example, `x` in
  // `x.f()`.
  std::unique_ptr<Expr> target_;
  // Required. The name of the function or method being called.
  std::string function_;
  // The arguments.
  std::vector<Expr> args_;
};

// A list creation expression.
//
// Lists may either be homogenous, e.g. `[1, 2, 3]`, or heterogeneous, e.g.
// `dyn([1, 'hello', 2.0])`
// (--
// TODO(issues/5): Determine how to disable heterogeneous types as a feature
// of type-checking rather than through the language construct 'dyn'.
// --)
class CreateList {
 public:
  CreateList() {}
  explicit CreateList(std::vector<Expr> elements)
      : elements_(std::move(elements)) {}

  void set_elements(std::vector<Expr> elements) {
    elements_ = std::move(elements);
  }

  const std::vector<Expr>& elements() const { return elements_; }

  std::vector<Expr>& mutable_elements() { return elements_; }

 private:
  // The elements part of the list.
  std::vector<Expr> elements_;
};

// A map or message creation expression.
//
// Maps are constructed as `{'key_name': 'value'}`. Message construction is
// similar, but prefixed with a type name and composed of field ids:
// `types.MyType{field_id: 'value'}`.
class CreateStruct {
 public:
  // Represents an entry.
  class Entry {
   public:
    Entry(int64_t id,
          absl::variant<std::string, std::unique_ptr<Expr>> key_kind,
          std::unique_ptr<Expr> value)
        : id_(id), key_kind_(std::move(key_kind)), value_(std::move(value)) {}

    void set_id(int64_t id) { id_ = id; }

    void set_key_kind(
        absl::variant<std::string, std::unique_ptr<Expr>> key_kind) {
      key_kind_ = std::move(key_kind);
    }

    void set_value(std::unique_ptr<Expr> value) { value_ = std::move(value); }

    int64_t id() const { return id_; }

    const absl::variant<std::string, std::unique_ptr<Expr>>& key_kind() const {
      return key_kind_;
    }

    absl::variant<std::string, std::unique_ptr<Expr>>& mutable_key_kind() {
      return key_kind_;
    }

    const Expr* value() const { return value_.get(); }

    Expr& mutable_value() {
      ABSL_ASSERT(value_ != nullptr);
      return *value_;
    }

   private:
    // Required. An id assigned to this node by the parser which is unique
    // in a given expression tree. This is used to associate type
    // information and other attributes to the node.
    int64_t id_;
    // The `Entry` key kinds.
    absl::variant<std::string, std::unique_ptr<Expr>> key_kind_;
    // Required. The value assigned to the key.
    std::unique_ptr<Expr> value_;
  };

  CreateStruct() {}
  CreateStruct(std::string message_name, std::vector<Entry> entries)
      : message_name_(std::move(message_name)), entries_(std::move(entries)) {}

  void set_message_name(std::string message_name) {
    message_name_ = std::move(message_name);
  }

  void set_entries(std::vector<Entry> entries) {
    entries_ = std::move(entries);
  }

  const std::vector<Entry>& entries() const { return entries_; }

  std::vector<Entry>& mutable_entries() { return entries_; }

 private:
  // The type name of the message to be created, empty when creating map
  // literals.
  std::string message_name_;
  // The entries in the creation expression.
  std::vector<Entry> entries_;
};

// A comprehension expression applied to a list or map.
//
// Comprehensions are not part of the core syntax, but enabled with macros.
// A macro matches a specific call signature within a parsed AST and replaces
// the call with an alternate AST block. Macro expansion happens at parse
// time.
//
// The following macros are supported within CEL:
//
// Aggregate type macros may be applied to all elements in a list or all keys
// in a map:
//
// *  `all`, `exists`, `exists_one` -  test a predicate expression against
//    the inputs and return `true` if the predicate is satisfied for all,
//    any, or only one value `list.all(x, x < 10)`.
// *  `filter` - test a predicate expression against the inputs and return
//    the subset of elements which satisfy the predicate:
//    `payments.filter(p, p > 1000)`.
// *  `map` - apply an expression to all elements in the input and return the
//    output aggregate type: `[1, 2, 3].map(i, i * i)`.
//
// The `has(m.x)` macro tests whether the property `x` is present in struct
// `m`. The semantics of this macro depend on the type of `m`. For proto2
// messages `has(m.x)` is defined as 'defined, but not set`. For proto3, the
// macro tests whether the property is set to its default. For map and struct
// types, the macro tests whether the property `x` is defined on `m`.
//
// Comprehension evaluation can be best visualized as the following
// pseudocode:
//
// ```
// let `accu_var` = `accu_init`
// for (let `iter_var` in `iter_range`) {
//   if (!`loop_condition`) {
//     break
//   }
//   `accu_var` = `loop_step`
// }
// return `result`
// ```
//
// (--
// TODO(issues/5): ensure comprehensions work equally well on maps and
// messages.
// --)
class Comprehension {
 public:
  Comprehension() {}
  Comprehension(std::string iter_var, std::unique_ptr<Expr> iter_range,
                std::string accu_var, std::unique_ptr<Expr> accu_init,
                std::unique_ptr<Expr> loop_condition,
                std::unique_ptr<Expr> loop_step, std::unique_ptr<Expr> result)
      : iter_var_(std::move(iter_var)),
        iter_range_(std::move(iter_range)),
        accu_var_(std::move(accu_var)),
        accu_init_(std::move(accu_init)),
        loop_condition_(std::move(loop_condition)),
        loop_step_(std::move(loop_step)),
        result_(std::move(result)) {}

  void set_iter_var(std::string iter_var) { iter_var_ = std::move(iter_var); }

  void set_iter_range(std::unique_ptr<Expr> iter_range) {
    iter_range_ = std::move(iter_range);
  }

  void set_accu_var(std::string accu_var) { accu_var_ = std::move(accu_var); }

  void set_accu_init(std::unique_ptr<Expr> accu_init) {
    accu_init_ = std::move(accu_init);
  }

  void set_loop_condition(std::unique_ptr<Expr> loop_condition) {
    loop_condition_ = std::move(loop_condition);
  }

  void set_loop_step(std::unique_ptr<Expr> loop_step) {
    loop_step_ = std::move(loop_step);
  }

  void set_result(std::unique_ptr<Expr> result) { result_ = std::move(result); }

  const std::string& iter_var() const { return iter_var_; }

  const Expr* iter_range() const { return iter_range_.get(); }

  Expr& mutable_iter_range() {
    ABSL_ASSERT(iter_range_ != nullptr);
    return *iter_range_;
  }

  const std::string& accu_var() const { return accu_var_; }

  const Expr* accu_init() const { return accu_init_.get(); }

  Expr& mutable_accu_init() {
    ABSL_ASSERT(accu_init_ != nullptr);
    return *accu_init_;
  }

  const Expr* loop_condition() const { return loop_condition_.get(); }

  Expr& mutable_loop_condition() {
    ABSL_ASSERT(loop_condition_ != nullptr);
    return *loop_condition_;
  }

  const Expr* loop_step() const { return loop_step_.get(); }

  Expr& mutable_loop_step() {
    ABSL_ASSERT(loop_step_ != nullptr);
    return *loop_step_;
  }

  const Expr* result() const { return result_.get(); }

  Expr& mutable_result() {
    ABSL_ASSERT(result_ != nullptr);
    return *result_;
  }

 private:
  // The name of the iteration variable.
  std::string iter_var_;

  // The range over which var iterates.
  std::unique_ptr<Expr> iter_range_;

  // The name of the variable used for accumulation of the result.
  std::string accu_var_;

  // The initial value of the accumulator.
  std::unique_ptr<Expr> accu_init_;

  // An expression which can contain iter_var and accu_var.
  //
  // Returns false when the result has been computed and may be used as
  // a hint to short-circuit the remainder of the comprehension.
  std::unique_ptr<Expr> loop_condition_;

  // An expression which can contain iter_var and accu_var.
  //
  // Computes the next value of accu_var.
  std::unique_ptr<Expr> loop_step_;

  // An expression which can contain accu_var.
  //
  // Computes the result.
  std::unique_ptr<Expr> result_;
};

using ExprKind = absl::variant<Constant, Ident, Select, Call, CreateList,
                               CreateStruct, Comprehension>;

// Analogous to google::api::expr::v1alpha1::Expr
// An abstract representation of a common expression.
//
// Expressions are abstractly represented as a collection of identifiers,
// select statements, function calls, literals, and comprehensions. All
// operators with the exception of the '.' operator are modelled as function
// calls. This makes it easy to represent new operators into the existing AST.
//
// All references within expressions must resolve to a [Decl][] provided at
// type-check for an expression to be valid. A reference may either be a bare
// identifier `name` or a qualified identifier `google.api.name`. References
// may either refer to a value or a function declaration.
//
// For example, the expression `google.api.name.startsWith('expr')` references
// the declaration `google.api.name` within a [Expr.Select][] expression, and
// the function declaration `startsWith`.
// Move-only type.
class Expr {
 public:
  Expr() {}
  Expr(int64_t id, ExprKind expr_kind)
      : id_(id), expr_kind_(std::move(expr_kind)) {}

  Expr(Expr&& rhs) = default;
  Expr& operator=(Expr&& rhs) = default;

  void set_id(int64_t id) { id_ = id; }

  void set_expr_kind(ExprKind expr_kind) { expr_kind_ = std::move(expr_kind); }

  int64_t id() const { return id_; }

  const ExprKind& expr_kind() const { return expr_kind_; }

  ExprKind& mutable_expr_kind() { return expr_kind_; }

 private:
  // Required. An id assigned to this node by the parser which is unique in a
  // given expression tree. This is used to associate type information and other
  // attributes to a node in the parse tree.
  int64_t id_ = 0;
  // Required. Variants of expressions.
  ExprKind expr_kind_;
};

// Source information collected at parse time.
class SourceInfo {
 public:
  SourceInfo() {}
  SourceInfo(std::string syntax_version, std::string location,
             std::vector<int32_t> line_offsets,
             absl::flat_hash_map<int64_t, int32_t> positions,
             absl::flat_hash_map<int64_t, Expr> macro_calls)
      : syntax_version_(std::move(syntax_version)),
        location_(std::move(location)),
        line_offsets_(std::move(line_offsets)),
        positions_(std::move(positions)),
        macro_calls_(std::move(macro_calls)) {}

  void set_syntax_version(std::string syntax_version) {
    syntax_version_ = std::move(syntax_version);
  }

  void set_location(std::string location) { location_ = std::move(location); }

  void set_line_offsets(std::vector<int32_t> line_offsets) {
    line_offsets_ = std::move(line_offsets);
  }

  void set_positions(absl::flat_hash_map<int64_t, int32_t> positions) {
    positions_ = std::move(positions);
  }

  void set_macro_calls(absl::flat_hash_map<int64_t, Expr> macro_calls) {
    macro_calls_ = std::move(macro_calls);
  }

  const std::string& syntax_version() const { return syntax_version_; }

  const std::string& location() const { return location_; }

  const std::vector<int32_t>& line_offsets() const { return line_offsets_; }

  std::vector<int32_t>& mutable_line_offsets() { return line_offsets_; }

  const absl::flat_hash_map<int64_t, int32_t>& positions() const {
    return positions_;
  }

  absl::flat_hash_map<int64_t, int32_t>& mutable_positions() {
    return positions_;
  }

  const absl::flat_hash_map<int64_t, Expr>& macro_calls() const {
    return macro_calls_;
  }

  absl::flat_hash_map<int64_t, Expr>& mutable_macro_calls() {
    return macro_calls_;
  }

 private:
  // The syntax version of the source, e.g. `cel1`.
  std::string syntax_version_;

  // The location name. All position information attached to an expression is
  // relative to this location.
  //
  // The location could be a file, UI element, or similar. For example,
  // `acme/app/AnvilPolicy.cel`.
  std::string location_;

  // Monotonically increasing list of code point offsets where newlines
  // `\n` appear.
  //
  // The line number of a given position is the index `i` where for a given
  // `id` the `line_offsets[i] < id_positions[id] < line_offsets[i+1]`. The
  // column may be derivd from `id_positions[id] - line_offsets[i]`.
  //
  // TODO(issues/5): clarify this documentation
  std::vector<int32_t> line_offsets_;

  // A map from the parse node id (e.g. `Expr.id`) to the code point offset
  // within source.
  absl::flat_hash_map<int64_t, int32_t> positions_;

  // A map from the parse node id where a macro replacement was made to the
  // call `Expr` that resulted in a macro expansion.
  //
  // For example, `has(value.field)` is a function call that is replaced by a
  // `test_only` field selection in the AST. Likewise, the call
  // `list.exists(e, e > 10)` translates to a comprehension expression. The key
  // in the map corresponds to the expression id of the expanded macro, and the
  // value is the call `Expr` that was replaced.
  absl::flat_hash_map<int64_t, Expr> macro_calls_;
};

// Analogous to google::api::expr::v1alpha1::ParsedExpr
// An expression together with source information as returned by the parser.
// Move-only type.
class ParsedExpr {
 public:
  ParsedExpr() {}
  ParsedExpr(Expr expr, SourceInfo source_info)
      : expr_(std::move(expr)), source_info_(std::move(source_info)) {}

  ParsedExpr(ParsedExpr&& rhs) = default;
  ParsedExpr& operator=(ParsedExpr&& rhs) = default;

  void set_expr(Expr expr) { expr_ = std::move(expr); }

  void set_source_info(SourceInfo source_info) {
    source_info_ = std::move(source_info);
  }

  const Expr& expr() const { return expr_; }

  Expr& mutable_expr() { return expr_; }

  const SourceInfo& source_info() const { return source_info_; }

  SourceInfo& mutable_source_info() { return source_info_; }

 private:
  // The parsed expression.
  Expr expr_;
  // The source info derived from input that generated the parsed `expr`.
  SourceInfo source_info_;
};

// CEL primitive types.
enum class PrimitiveType {
  // Unspecified type.
  kPrimitiveTypeUnspecified = 0,
  // Boolean type.
  kBool = 1,
  // Int64 type.
  //
  // Proto-based integer values are widened to int64_t.
  kInt64 = 2,
  // Uint64 type.
  //
  // Proto-based unsigned integer values are widened to uint64_t.
  kUint64 = 3,
  // Double type.
  //
  // Proto-based float values are widened to double values.
  kDouble = 4,
  // String type.
  kString = 5,
  // Bytes type.
  kBytes = 6,
};

// Well-known protobuf types treated with first-class support in CEL.
//
// TODO(issues/5): represent well-known via abstract types (or however)
//   they will be named.
enum class WellKnownType {
  // Unspecified type.
  kWellKnownTypeUnspecified = 0,
  // Well-known protobuf.Any type.
  //
  // Any types are a polymorphic message type. During type-checking they are
  // treated like `DYN` types, but at runtime they are resolved to a specific
  // message type specified at evaluation time.
  kAny = 1,
  // Well-known protobuf.Timestamp type, internally referenced as `timestamp`.
  kTimestamp = 2,
  // Well-known protobuf.Duration type, internally referenced as `duration`.
  kDuration = 3,
};

class Type;

// List type with typed elements, e.g. `list<example.proto.MyMessage>`.
class ListType {
  explicit ListType(std::unique_ptr<Type> elem_type)
      : elem_type_(std::move(elem_type)) {}

  void set_elem_type(std::unique_ptr<Type> elem_type) {
    elem_type_ = std::move(elem_type);
  }

  const Type* elem_type() const { return elem_type_.get(); }

  Type& mutable_elem_type() {
    ABSL_ASSERT(elem_type_ != nullptr);
    return *elem_type_;
  }

 private:
  std::unique_ptr<Type> elem_type_;
};

// Map type with parameterized key and value types, e.g. `map<string, int>`.
class MapType {
 public:
  MapType(std::unique_ptr<Type> key_type, std::unique_ptr<Type> value_type)
      : key_type_(std::move(key_type)), value_type_(std::move(value_type)) {}

  void set_key_type(std::unique_ptr<Type> key_type) {
    key_type_ = std::move(key_type);
  }

  void set_value_type(std::unique_ptr<Type> value_type) {
    value_type_ = std::move(value_type);
  }

  const Type* key_type() const { return key_type_.get(); }

  const Type* value_type() const { return value_type_.get(); }

  Type& mutable_key_type() {
    ABSL_ASSERT(key_type_ != nullptr);
    return *key_type_;
  }

  Type& mutable_value_type() {
    ABSL_ASSERT(value_type_ != nullptr);
    return *value_type_;
  }

 private:
  // The type of the key.
  std::unique_ptr<Type> key_type_;

  // The type of the value.
  std::unique_ptr<Type> value_type_;
};

// Function type with result and arg types.
//
// (--
// NOTE: function type represents a lambda-style argument to another function.
// Supported through macros, but not yet a first-class concept in CEL.
// --)
class FunctionType {
 public:
  FunctionType(std::unique_ptr<Type> result_type, std::vector<Type> arg_types)
      : result_type_(std::move(result_type)),
        arg_types_(std::move(arg_types)) {}

  void set_result_type(std::unique_ptr<Type> result_type) {
    result_type_ = std::move(result_type);
  }

  void set_arg_types(std::vector<Type> arg_types) {
    arg_types_ = std::move(arg_types);
  }

  const Type* result_type() const { return result_type_.get(); }

  Type& mutable_result_type() {
    ABSL_ASSERT(result_type_.get() != nullptr);
    return *result_type_;
  }

  const std::vector<Type>& arg_types() const { return arg_types_; }

  std::vector<Type>& mutable_arg_types() { return arg_types_; }

 private:
  // Result type of the function.
  std::unique_ptr<Type> result_type_;

  // Argument types of the function.
  std::vector<Type> arg_types_;
};

// Application defined abstract type.
//
// TODO(issues/5): decide on final naming for this.
class AbstractType {
  AbstractType(std::string name, std::vector<Type> parameter_types)
      : name_(std::move(name)), parameter_types_(std::move(parameter_types)) {}

  void set_name(std::string name) { name_ = std::move(name); }

  void set_parameter_types(std::vector<Type> parameter_types) {
    parameter_types_ = std::move(parameter_types);
  }

  const std::string& name() const { return name_; }

  const std::vector<Type>& parameter_types() const { return parameter_types_; }

  std::vector<Type>& mutable_parameter_types() { return parameter_types_; }

 private:
  // The fully qualified name of this abstract type.
  std::string name_;

  // Parameter types for this abstract type.
  std::vector<Type> parameter_types_;
};

// Wrapper of a primitive type, e.g. `google.protobuf.Int64Value`.
class PrimitiveTypeWrapper {
 public:
  explicit PrimitiveTypeWrapper(PrimitiveType type) : type_(std::move(type)) {}

  void set_type(PrimitiveType type) { type_ = std::move(type); }

  const PrimitiveType& type() const { return type_; }

  PrimitiveType& type() { return type_; }

 private:
  PrimitiveType type_;
};

// Protocol buffer message type.
//
// The `message_type` string specifies the qualified message type name. For
// example, `google.plus.Profile`.
class MessageType {
 public:
  explicit MessageType(std::string type) : type_(std::move(type)) {}

  void set_type(std::string type) { type_ = std::move(type); }

  const std::string& type() { return type_; }

 private:
  std::string type_;
};

// Type param type.
//
// The `type_param` string specifies the type parameter name, e.g. `list<E>`
// would be a `list_type` whose element type was a `type_param` type
// named `E`.
class ParamType {
 public:
  explicit ParamType(std::string type) : type_(std::move(type)) {}

  void set_type(std::string type) { type_ = std::move(type); }

  const std::string& type() { return type_; }

 private:
  std::string type_;
};

// Error type.
//
// During type-checking if an expression is an error, its type is propagated
// as the `ERROR` type. This permits the type-checker to discover other
// errors present in the expression.
enum class ErrorType { kErrorTypeValue = 0 };

using DynamicType = absl::monostate;

using TypeKind =
    absl::variant<DynamicType, NullValue, PrimitiveType, PrimitiveTypeWrapper,
                  WellKnownType, ListType, MapType, FunctionType, MessageType,
                  ParamType, std::unique_ptr<Type>, ErrorType, AbstractType>;

// Analogous to google::api::expr::v1alpha1::Type.
// Represents a CEL type.
//
// TODO(issues/5): align with value.proto
class Type {
 public:
  explicit Type(TypeKind type_kind) : type_kind_(std::move(type_kind)) {}

  Type(Type&& rhs) = default;
  Type& operator=(Type&& rhs) = default;

  void set_type_kind(TypeKind type_kind) { type_kind_ = std::move(type_kind); }

  const TypeKind& type_kind() const { return type_kind_; }

  TypeKind& mutable_type_kind() { return type_kind_; }

 private:
  TypeKind type_kind_;
};

// Describes a resolved reference to a declaration.
class Reference {
 public:
  Reference(std::string name, std::vector<std::string> overload_id,
            Constant value)
      : name_(std::move(name)),
        overload_id_(std::move(overload_id)),
        value_(std::move(value)) {}

  void set_name(std::string name) { name_ = std::move(name); }

  void set_overload_id(std::vector<std::string> overload_id) {
    overload_id_ = std::move(overload_id);
  }

  void set_value(Constant value) { value_ = std::move(value); }

  const std::string& name() const { return name_; }

  const std::vector<std::string>& overload_id() const { return overload_id_; }

  const Constant& value() const { return value_; }

  std::vector<std::string>& mutable_overload_id() { return overload_id_; }

  Constant& mutable_value() { return value_; }

 private:
  // The fully qualified name of the declaration.
  std::string name_;
  // For references to functions, this is a list of `Overload.overload_id`
  // values which match according to typing rules.
  //
  // If the list has more than one element, overload resolution among the
  // presented candidates must happen at runtime because of dynamic types. The
  // type checker attempts to narrow down this list as much as possible.
  //
  // Empty if this is not a reference to a [Decl.FunctionDecl][].
  std::vector<std::string> overload_id_;
  // For references to constants, this may contain the value of the
  // constant if known at compile time.
  Constant value_;
};

// Analogous to google::api::expr::v1alpha1::CheckedExpr
// A CEL expression which has been successfully type checked.
// Move-only type.
class CheckedExpr {
 public:
  CheckedExpr() {}
  CheckedExpr(absl::flat_hash_map<int64_t, Reference> reference_map,
              absl::flat_hash_map<int64_t, Type> type_map,
              SourceInfo source_info, std::string expr_version, Expr expr)
      : reference_map_(std::move(reference_map)),
        type_map_(std::move(type_map)),
        source_info_(std::move(source_info)),
        expr_version_(std::move(expr_version)),
        expr_(std::move(expr)) {}

  CheckedExpr(CheckedExpr&& rhs) = default;
  CheckedExpr& operator=(CheckedExpr&& rhs) = default;

  void set_reference_map(
      absl::flat_hash_map<int64_t, Reference> reference_map) {
    reference_map_ = std::move(reference_map);
  }

  void set_type_map(absl::flat_hash_map<int64_t, Type> type_map) {
    type_map_ = std::move(type_map);
  }

  void set_source_info(SourceInfo source_info) {
    source_info_ = std::move(source_info);
  }

  void set_expr_version(std::string expr_version) {
    expr_version_ = std::move(expr_version);
  }

  void set_expr(Expr expr) { expr_ = std::move(expr); }

  const absl::flat_hash_map<int64_t, Reference>& reference_map() const {
    return reference_map_;
  }

  absl::flat_hash_map<int64_t, Reference>& mutable_reference_map() {
    return reference_map_;
  }

  const absl::flat_hash_map<int64_t, Type>& type_map() const {
    return type_map_;
  }

  absl::flat_hash_map<int64_t, Type>& mutable_type_map() { return type_map_; }

  const SourceInfo& source_info() const { return source_info_; }

  SourceInfo& mutable_source_info() { return source_info_; }

  const std::string& expr_version() const { return expr_version_; }

  const Expr& expr() const { return expr_; }

  Expr& mutable_expr() { return expr_; }

 private:
  // A map from expression ids to resolved references.
  //
  // The following entries are in this table:
  //
  // - An Ident or Select expression is represented here if it resolves to a
  //   declaration. For instance, if `a.b.c` is represented by
  //   `select(select(id(a), b), c)`, and `a.b` resolves to a declaration,
  //   while `c` is a field selection, then the reference is attached to the
  //   nested select expression (but not to the id or or the outer select).
  //   In turn, if `a` resolves to a declaration and `b.c` are field selections,
  //   the reference is attached to the ident expression.
  // - Every Call expression has an entry here, identifying the function being
  //   called.
  // - Every CreateStruct expression for a message has an entry, identifying
  //   the message.
  absl::flat_hash_map<int64_t, Reference> reference_map_;
  // A map from expression ids to types.
  //
  // Every expression node which has a type different than DYN has a mapping
  // here. If an expression has type DYN, it is omitted from this map to save
  // space.
  absl::flat_hash_map<int64_t, Type> type_map_;
  // The source info derived from input that generated the parsed `expr` and
  // any optimizations made during the type-checking pass.
  SourceInfo source_info_;
  // The expr version indicates the major / minor version number of the `expr`
  // representation.
  //
  // The most common reason for a version change will be to indicate to the CEL
  // runtimes that transformations have been performed on the expr during static
  // analysis. In some cases, this will save the runtime the work of applying
  // the same or similar transformations prior to evaluation.
  std::string expr_version_;
  // The checked expression. Semantically equivalent to the parsed `expr`, but
  // may have structural differences.
  Expr expr_;
};

}  // namespace cel::ast::internal

#endif  // THIRD_PARTY_CEL_CPP_BASE_AST_H_
