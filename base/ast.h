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

#include "absl/container/flat_hash_map.h"
#include "absl/time/time.h"
#include "absl/types/variant.h"
namespace cel::ast::internal {

enum class NullValue { kNullValue = 0 };

// A holder class to differentiate between CEL string and CEL bytes constants.
struct Bytes {
  std::string bytes;

  bool operator==(const Bytes& other) const { return bytes == other.bytes; }
};

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
using ConstantKind =
    absl::variant<NullValue, bool, int64_t, uint64_t, double, std::string,
                  Bytes, absl::Duration, absl::Time>;

class Constant {
 public:
  constexpr Constant() {}
  explicit Constant(ConstantKind constant_kind)
      : constant_kind_(std::move(constant_kind)) {}

  void set_constant_kind(ConstantKind constant_kind) {
    constant_kind_ = std::move(constant_kind);
  }

  const ConstantKind& constant_kind() const { return constant_kind_; }

  ConstantKind& mutable_constant_kind() { return constant_kind_; }

  bool has_null_value() const {
    return absl::holds_alternative<NullValue>(constant_kind_);
  }

  NullValue null_value() const {
    auto* value = absl::get_if<NullValue>(&constant_kind_);
    if (value != nullptr) {
      return *value;
    }
    return NullValue::kNullValue;
  }

  void set_null_value(NullValue null_value) { constant_kind_ = null_value; }

  bool has_bool_value() const {
    return absl::holds_alternative<bool>(constant_kind_);
  }

  bool bool_value() const {
    auto* value = absl::get_if<bool>(&constant_kind_);
    if (value != nullptr) {
      return *value;
    }
    return false;
  }

  void set_bool_value(bool bool_value) { constant_kind_ = bool_value; }

  bool has_int64_value() const {
    return absl::holds_alternative<int64_t>(constant_kind_);
  }

  int64_t int64_value() const {
    auto* value = absl::get_if<int64_t>(&constant_kind_);
    if (value != nullptr) {
      return *value;
    }
    return 0;
  }

  void set_int64_value(int64_t int64_value) { constant_kind_ = int64_value; }

  bool has_uint64_value() const {
    return absl::holds_alternative<uint64_t>(constant_kind_);
  }

  uint64_t uint64_value() const {
    auto* value = absl::get_if<uint64_t>(&constant_kind_);
    if (value != nullptr) {
      return *value;
    }
    return 0;
  }

  void set_uint64_value(uint64_t uint64_value) {
    constant_kind_ = uint64_value;
  }

  bool has_double_value() const {
    return absl::holds_alternative<double>(constant_kind_);
  }

  double double_value() const {
    auto* value = absl::get_if<double>(&constant_kind_);
    if (value != nullptr) {
      return *value;
    }
    return 0;
  }

  void set_double_value(double double_value) { constant_kind_ = double_value; }

  bool has_string_value() const {
    return absl::holds_alternative<std::string>(constant_kind_);
  }

  const std::string& string_value() const {
    auto* value = absl::get_if<std::string>(&constant_kind_);
    if (value != nullptr) {
      return *value;
    }
    static std::string* default_string_value_ = new std::string("");
    return *default_string_value_;
  }

  void set_string_value(std::string string_value) {
    constant_kind_ = string_value;
  }

  bool has_bytes_value() const {
    return absl::holds_alternative<Bytes>(constant_kind_);
  }

  const std::string& bytes_value() const {
    auto* value = absl::get_if<Bytes>(&constant_kind_);
    if (value != nullptr) {
      return value->bytes;
    }
    static std::string* default_string_value_ = new std::string("");
    return *default_string_value_;
  }

  void set_bytes_value(std::string bytes_value) {
    constant_kind_ = Bytes{std::move(bytes_value)};
  }

  bool has_duration_value() const {
    return absl::holds_alternative<absl::Duration>(constant_kind_);
  }

  void set_duration_value(absl::Duration duration_value) {
    constant_kind_ = std::move(duration_value);
  }

  const absl::Duration& duration_value() const {
    auto* value = absl::get_if<absl::Duration>(&constant_kind_);
    if (value != nullptr) {
      return *value;
    }
    static absl::Duration default_duration_;
    return default_duration_;
  }

  bool has_time_value() const {
    return absl::holds_alternative<absl::Time>(constant_kind_);
  }

  const absl::Time& time_value() const {
    auto* value = absl::get_if<absl::Time>(&constant_kind_);
    if (value != nullptr) {
      return *value;
    }
    static absl::Time default_time_;
    return default_time_;
  }

  void set_time_value(absl::Time time_value) {
    constant_kind_ = std::move(time_value);
  }

  bool operator==(const Constant& other) const {
    return constant_kind_ == other.constant_kind_;
  }

 private:
  ConstantKind constant_kind_;
};

class Expr;

// An identifier expression. e.g. `request`.
class Ident {
 public:
  Ident() {}
  explicit Ident(std::string name) : name_(std::move(name)) {}

  void set_name(std::string name) { name_ = std::move(name); }

  const std::string& name() const { return name_; }

  bool operator==(const Ident& other) const { return name_ == other.name_; }

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
  Select() {}
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

  bool has_operand() const { return operand_ != nullptr; }

  const Expr& operand() const;

  Expr& mutable_operand() {
    if (operand_ == nullptr) {
      operand_ = std::make_unique<Expr>();
    }
    return *operand_;
  }

  const std::string& field() const { return field_; }

  bool test_only() const { return test_only_; }

  bool operator==(const Select& other) const;

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
  bool test_only_ = false;
};

// A call expression, including calls to predefined functions and operators.
//
// For example, `value == 10`, `size(map_value)`.
// (-- TODO(issues/5): Convert built-in globals to instance methods --)
class Call {
 public:
  Call();
  Call(std::unique_ptr<Expr> target, std::string function,
       std::vector<Expr> args);

  void set_target(std::unique_ptr<Expr> target) { target_ = std::move(target); }

  void set_function(std::string function) { function_ = std::move(function); }

  void set_args(std::vector<Expr> args);

  bool has_target() const { return target_ != nullptr; }

  const Expr& target() const;

  Expr& mutable_target() {
    if (target_ == nullptr) {
      target_ = std::make_unique<Expr>();
    }
    return *target_;
  }

  const std::string& function() const { return function_; }

  const std::vector<Expr>& args() const { return args_; }

  std::vector<Expr>& mutable_args() { return args_; }

  bool operator==(const Call& other) const;

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
  CreateList();
  explicit CreateList(std::vector<Expr> elements);

  void set_elements(std::vector<Expr> elements);

  const std::vector<Expr>& elements() const { return elements_; }

  std::vector<Expr>& mutable_elements() { return elements_; }

  bool operator==(const CreateList& other) const;

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
    using KeyKind = absl::variant<std::string, std::unique_ptr<Expr>>;
    Entry() {}
    Entry(int64_t id, KeyKind key_kind, std::unique_ptr<Expr> value)
        : id_(id), key_kind_(std::move(key_kind)), value_(std::move(value)) {}

    void set_id(int64_t id) { id_ = id; }

    void set_key_kind(KeyKind key_kind) { key_kind_ = std::move(key_kind); }

    void set_value(std::unique_ptr<Expr> value) { value_ = std::move(value); }

    int64_t id() const { return id_; }

    const KeyKind& key_kind() const { return key_kind_; }

    KeyKind& mutable_key_kind() { return key_kind_; }

    bool has_field_key() const {
      return absl::holds_alternative<std::string>(key_kind_);
    }

    bool has_map_key() const {
      return absl::holds_alternative<std::unique_ptr<Expr>>(key_kind_);
    }

    const std::string& field_key() const {
      auto* value = absl::get_if<std::string>(&key_kind_);
      if (value != nullptr) {
        return *value;
      }
      static const std::string* default_field_key = new std::string;
      return *default_field_key;
    }

    void set_field_key(std::string field_key) {
      key_kind_ = std::move(field_key);
    }

    const Expr& map_key() const;

    Expr& mutable_map_key() {
      auto* value = absl::get_if<std::unique_ptr<Expr>>(&key_kind_);
      if (value != nullptr) {
        if (*value != nullptr) return **value;
      }
      key_kind_.emplace<std::unique_ptr<Expr>>(std::make_unique<Expr>());
      return *absl::get<std::unique_ptr<Expr>>(key_kind_);
    }

    bool has_value() const { return value_ != nullptr; }

    const Expr& value() const;

    Expr& mutable_value() {
      if (value_ == nullptr) {
        value_ = std::make_unique<Expr>();
      }
      return *value_;
    }

    bool operator==(const Entry& other) const;

    bool operator!=(const Entry& other) const { return !operator==(other); }

   private:
    // Required. An id assigned to this node by the parser which is unique
    // in a given expression tree. This is used to associate type
    // information and other attributes to the node.
    int64_t id_ = 0;
    // The `Entry` key kinds.
    KeyKind key_kind_;
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

  const std::string& message_name() const { return message_name_; }

  bool operator==(const CreateStruct& other) const {
    return message_name_ == other.message_name_ && entries_ == other.entries_;
  }

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

  bool has_iter_range() const { return iter_range_ != nullptr; }

  bool has_accu_init() const { return accu_init_ != nullptr; }

  bool has_loop_condition() const { return loop_condition_ != nullptr; }

  bool has_loop_step() const { return loop_step_ != nullptr; }

  bool has_result() const { return result_ != nullptr; }

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

  const Expr& iter_range() const;

  Expr& mutable_iter_range() {
    if (iter_range_ == nullptr) {
      iter_range_ = std::make_unique<Expr>();
    }
    return *iter_range_;
  }

  const std::string& accu_var() const { return accu_var_; }

  const Expr& accu_init() const;

  Expr& mutable_accu_init() {
    if (accu_init_ == nullptr) {
      accu_init_ = std::make_unique<Expr>();
    }
    return *accu_init_;
  }

  const Expr& loop_condition() const;

  Expr& mutable_loop_condition() {
    if (loop_condition_ == nullptr) {
      loop_condition_ = std::make_unique<Expr>();
    }
    return *loop_condition_;
  }

  const Expr& loop_step() const;

  Expr& mutable_loop_step() {
    if (loop_step_ == nullptr) {
      loop_step_ = std::make_unique<Expr>();
    }
    return *loop_step_;
  }

  const Expr& result() const;

  Expr& mutable_result() {
    if (result_ == nullptr) {
      result_ = std::make_unique<Expr>();
    }
    return *result_;
  }

  bool operator==(const Comprehension& other) const;

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

// Even though, the Expr proto does not allow for an unset, macro calls in the
// way they are used today sometimes elide parts of the AST if its
// unchanged/uninteresting.
using ExprKind =
    absl::variant<absl::monostate /* unset */, Constant, Ident, Select, Call,
                  CreateList, CreateStruct, Comprehension>;

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

  bool has_const_expr() const {
    return absl::holds_alternative<Constant>(expr_kind_);
  }

  bool has_ident_expr() const {
    return absl::holds_alternative<Ident>(expr_kind_);
  }

  bool has_select_expr() const {
    return absl::holds_alternative<Select>(expr_kind_);
  }

  bool has_call_expr() const {
    return absl::holds_alternative<Call>(expr_kind_);
  }

  bool has_list_expr() const {
    return absl::holds_alternative<CreateList>(expr_kind_);
  }

  bool has_struct_expr() const {
    return absl::holds_alternative<CreateStruct>(expr_kind_);
  }

  bool has_comprehension_expr() const {
    return absl::holds_alternative<Comprehension>(expr_kind_);
  }

  const Constant& const_expr() const {
    auto* value = absl::get_if<Constant>(&expr_kind_);
    if (value != nullptr) {
      return *value;
    }
    static const Constant* default_constant = new Constant;
    return *default_constant;
  }

  Constant& mutable_const_expr() {
    auto* value = absl::get_if<Constant>(&expr_kind_);
    if (value != nullptr) {
      return *value;
    }
    expr_kind_.emplace<Constant>();
    return absl::get<Constant>(expr_kind_);
  }

  const Ident& ident_expr() const {
    auto* value = absl::get_if<Ident>(&expr_kind_);
    if (value != nullptr) {
      return *value;
    }
    static const Ident* default_ident = new Ident;
    return *default_ident;
  }

  Ident& mutable_ident_expr() {
    auto* value = absl::get_if<Ident>(&expr_kind_);
    if (value != nullptr) {
      return *value;
    }
    expr_kind_.emplace<Ident>();
    return absl::get<Ident>(expr_kind_);
  }

  const Select& select_expr() const {
    auto* value = absl::get_if<Select>(&expr_kind_);
    if (value != nullptr) {
      return *value;
    }
    static const Select* default_select = new Select;
    return *default_select;
  }

  Select& mutable_select_expr() {
    auto* value = absl::get_if<Select>(&expr_kind_);
    if (value != nullptr) {
      return *value;
    }
    expr_kind_.emplace<Select>();
    return absl::get<Select>(expr_kind_);
  }

  const Call& call_expr() const {
    auto* value = absl::get_if<Call>(&expr_kind_);
    if (value != nullptr) {
      return *value;
    }
    static const Call* default_call = new Call;
    return *default_call;
  }

  Call& mutable_call_expr() {
    auto* value = absl::get_if<Call>(&expr_kind_);
    if (value != nullptr) {
      return *value;
    }
    expr_kind_.emplace<Call>();
    return absl::get<Call>(expr_kind_);
  }

  const CreateList& list_expr() const {
    auto* value = absl::get_if<CreateList>(&expr_kind_);
    if (value != nullptr) {
      return *value;
    }
    static const CreateList* default_create_list = new CreateList;
    return *default_create_list;
  }

  CreateList& mutable_list_expr() {
    auto* value = absl::get_if<CreateList>(&expr_kind_);
    if (value != nullptr) {
      return *value;
    }
    expr_kind_.emplace<CreateList>();
    return absl::get<CreateList>(expr_kind_);
  }

  const CreateStruct& struct_expr() const {
    auto* value = absl::get_if<CreateStruct>(&expr_kind_);
    if (value != nullptr) {
      return *value;
    }
    static const CreateStruct* default_create_struct = new CreateStruct;
    return *default_create_struct;
  }

  CreateStruct& mutable_struct_expr() {
    auto* value = absl::get_if<CreateStruct>(&expr_kind_);
    if (value != nullptr) {
      return *value;
    }
    expr_kind_.emplace<CreateStruct>();
    return absl::get<CreateStruct>(expr_kind_);
  }

  const Comprehension& comprehension_expr() const {
    auto* value = absl::get_if<Comprehension>(&expr_kind_);
    if (value != nullptr) {
      return *value;
    }
    static const Comprehension* default_comprehension = new Comprehension;
    return *default_comprehension;
  }

  Comprehension& mutable_comprehension_expr() {
    auto* value = absl::get_if<Comprehension>(&expr_kind_);
    if (value != nullptr) {
      return *value;
    }
    expr_kind_.emplace<Comprehension>();
    return absl::get<Comprehension>(expr_kind_);
  }

  bool operator==(const Expr& other) const {
    return id_ == other.id_ && expr_kind_ == other.expr_kind_;
  }

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
 public:
  ListType() {}
  explicit ListType(std::unique_ptr<Type> elem_type)
      : elem_type_(std::move(elem_type)) {}

  void set_elem_type(std::unique_ptr<Type> elem_type) {
    elem_type_ = std::move(elem_type);
  }

  bool has_elem_type() const { return elem_type_ != nullptr; }

  const Type& elem_type() const;

  Type& mutable_elem_type() {
    if (elem_type_ == nullptr) {
      elem_type_ = std::make_unique<Type>();
    }
    return *elem_type_;
  }

  bool operator==(const ListType& other) const;

 private:
  std::unique_ptr<Type> elem_type_;
};

// Map type with parameterized key and value types, e.g. `map<string, int>`.
class MapType {
 public:
  MapType() {}
  MapType(std::unique_ptr<Type> key_type, std::unique_ptr<Type> value_type)
      : key_type_(std::move(key_type)), value_type_(std::move(value_type)) {}

  void set_key_type(std::unique_ptr<Type> key_type) {
    key_type_ = std::move(key_type);
  }

  void set_value_type(std::unique_ptr<Type> value_type) {
    value_type_ = std::move(value_type);
  }

  bool has_key_type() const { return key_type_ != nullptr; }

  bool has_value_type() const { return value_type_ != nullptr; }

  const Type& key_type() const;

  const Type& value_type() const;

  bool operator==(const MapType& other) const;

  Type& mutable_key_type() {
    if (key_type_ == nullptr) {
      key_type_ = std::make_unique<Type>();
    }
    return *key_type_;
  }

  Type& mutable_value_type() {
    if (value_type_ == nullptr) {
      value_type_ = std::make_unique<Type>();
    }
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
  FunctionType();
  FunctionType(std::unique_ptr<Type> result_type, std::vector<Type> arg_types);

  void set_result_type(std::unique_ptr<Type> result_type) {
    result_type_ = std::move(result_type);
  }

  void set_arg_types(std::vector<Type> arg_types);

  bool has_result_type() const { return result_type_ != nullptr; }

  const Type& result_type() const;

  Type& mutable_result_type() {
    if (result_type_ == nullptr) {
      result_type_ = std::make_unique<Type>();
    }
    return *result_type_;
  }

  const std::vector<Type>& arg_types() const { return arg_types_; }

  std::vector<Type>& mutable_arg_types() { return arg_types_; }

  bool operator==(const FunctionType& other) const;

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
 public:
  AbstractType();
  AbstractType(std::string name, std::vector<Type> parameter_types);

  void set_name(std::string name) { name_ = std::move(name); }

  void set_parameter_types(std::vector<Type> parameter_types);

  const std::string& name() const { return name_; }

  const std::vector<Type>& parameter_types() const { return parameter_types_; }

  std::vector<Type>& mutable_parameter_types() { return parameter_types_; }

  bool operator==(const AbstractType& other) const;

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

  PrimitiveType& mutable_type() { return type_; }

  bool operator==(const PrimitiveTypeWrapper& other) const {
    return type_ == other.type_;
  }

 private:
  PrimitiveType type_;
};

// Protocol buffer message type.
//
// The `message_type` string specifies the qualified message type name. For
// example, `google.plus.Profile`.
class MessageType {
 public:
  MessageType() {}
  explicit MessageType(std::string type) : type_(std::move(type)) {}

  void set_type(std::string type) { type_ = std::move(type); }

  const std::string& type() const { return type_; }

  bool operator==(const MessageType& other) const {
    return type_ == other.type_;
  }

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
  ParamType() {}
  explicit ParamType(std::string type) : type_(std::move(type)) {}

  void set_type(std::string type) { type_ = std::move(type); }

  const std::string& type() const { return type_; }

  bool operator==(const ParamType& other) const { return type_ == other.type_; }

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
  Type() {}
  explicit Type(TypeKind type_kind) : type_kind_(std::move(type_kind)) {}

  Type(Type&& rhs) = default;
  Type& operator=(Type&& rhs) = default;

  void set_type_kind(TypeKind type_kind) { type_kind_ = std::move(type_kind); }

  const TypeKind& type_kind() const { return type_kind_; }

  TypeKind& mutable_type_kind() { return type_kind_; }

  bool has_dyn() const {
    return absl::holds_alternative<DynamicType>(type_kind_);
  }

  bool has_null() const {
    return absl::holds_alternative<NullValue>(type_kind_);
  }

  bool has_primitive() const {
    return absl::holds_alternative<PrimitiveType>(type_kind_);
  }

  bool has_wrapper() const {
    return absl::holds_alternative<PrimitiveTypeWrapper>(type_kind_);
  }

  bool has_well_known() const {
    return absl::holds_alternative<WellKnownType>(type_kind_);
  }

  bool has_list_type() const {
    return absl::holds_alternative<ListType>(type_kind_);
  }

  bool has_map_type() const {
    return absl::holds_alternative<MapType>(type_kind_);
  }

  bool has_function() const {
    return absl::holds_alternative<FunctionType>(type_kind_);
  }

  bool has_message_type() const {
    return absl::holds_alternative<MessageType>(type_kind_);
  }

  bool has_type_param() const {
    return absl::holds_alternative<ParamType>(type_kind_);
  }

  bool has_type() const {
    return absl::holds_alternative<std::unique_ptr<Type>>(type_kind_);
  }

  bool has_error() const {
    return absl::holds_alternative<ErrorType>(type_kind_);
  }

  bool has_abstract_type() const {
    return absl::holds_alternative<AbstractType>(type_kind_);
  }

  NullValue null() const {
    auto* value = absl::get_if<NullValue>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    return NullValue::kNullValue;
  }

  PrimitiveType primitive() const {
    auto* value = absl::get_if<PrimitiveType>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    return PrimitiveType::kPrimitiveTypeUnspecified;
  }

  PrimitiveType wrapper() const {
    auto* value = absl::get_if<PrimitiveTypeWrapper>(&type_kind_);
    if (value != nullptr) {
      return value->type();
    }
    return PrimitiveType::kPrimitiveTypeUnspecified;
  }

  WellKnownType well_known() const {
    auto* value = absl::get_if<WellKnownType>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    return WellKnownType::kWellKnownTypeUnspecified;
  }

  const ListType& list_type() const {
    auto* value = absl::get_if<ListType>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    static const ListType* default_list_type = new ListType();
    return *default_list_type;
  }

  const MapType& map_type() const {
    auto* value = absl::get_if<MapType>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    static const MapType* default_map_type = new MapType();
    return *default_map_type;
  }

  const FunctionType& function() const {
    auto* value = absl::get_if<FunctionType>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    static const FunctionType* default_function_type = new FunctionType();
    return *default_function_type;
  }

  const MessageType& message_type() const {
    auto* value = absl::get_if<MessageType>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    static const MessageType* default_message_type = new MessageType();
    return *default_message_type;
  }

  const ParamType& type_param() const {
    auto* value = absl::get_if<ParamType>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    static const ParamType* default_param_type = new ParamType();
    return *default_param_type;
  }

  const Type& type() const;

  ErrorType error_type() const {
    auto* value = absl::get_if<ErrorType>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    return ErrorType::kErrorTypeValue;
  }

  const AbstractType& abstract_type() const {
    auto* value = absl::get_if<AbstractType>(&type_kind_);
    if (value != nullptr) {
      return *value;
    }
    static const AbstractType* default_abstract_type = new AbstractType();
    return *default_abstract_type;
  }

  bool operator==(const Type& other) const {
    return type_kind_ == other.type_kind_;
  }

 private:
  TypeKind type_kind_;
};

// Describes a resolved reference to a declaration.
class Reference {
 public:
  Reference() {}

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

  const Constant& value() const {
    if (value_.has_value()) {
      return value_.value();
    }
    static const Constant* default_constant = new Constant;
    return *default_constant;
  }

  std::vector<std::string>& mutable_overload_id() { return overload_id_; }

  Constant& mutable_value() {
    if (!value_.has_value()) {
      value_.emplace();
    }
    return *value_;
  }

  bool has_value() const { return value_.has_value(); }

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
  absl::optional<Constant> value_;
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

////////////////////////////////////////////////////////////////////////
// Implementation details
////////////////////////////////////////////////////////////////////////

inline Call::Call() {}

inline Call::Call(std::unique_ptr<Expr> target, std::string function,
                  std::vector<Expr> args)
    : target_(std::move(target)),
      function_(std::move(function)),
      args_(std::move(args)) {}

inline void Call::set_args(std::vector<Expr> args) { args_ = std::move(args); }

inline CreateList::CreateList() {}

inline CreateList::CreateList(std::vector<Expr> elements)
    : elements_(std::move(elements)) {}

inline void CreateList::set_elements(std::vector<Expr> elements) {
  elements_ = std::move(elements);
}

inline bool CreateList::operator==(const CreateList& other) const {
  return elements_ == other.elements_;
}

inline FunctionType::FunctionType() {}

inline FunctionType::FunctionType(std::unique_ptr<Type> result_type,
                                  std::vector<Type> arg_types)
    : result_type_(std::move(result_type)), arg_types_(std::move(arg_types)) {}

inline void FunctionType::set_arg_types(std::vector<Type> arg_types) {
  arg_types_ = std::move(arg_types);
}

inline AbstractType::AbstractType() {}

inline AbstractType::AbstractType(std::string name,
                                  std::vector<Type> parameter_types)
    : name_(std::move(name)), parameter_types_(std::move(parameter_types)) {}

inline void AbstractType::set_parameter_types(
    std::vector<Type> parameter_types) {
  parameter_types_ = std::move(parameter_types);
}

inline bool AbstractType::operator==(const AbstractType& other) const {
  return name_ == other.name_ && parameter_types_ == other.parameter_types_;
}

}  // namespace cel::ast::internal

#endif  // THIRD_PARTY_CEL_CPP_BASE_AST_H_
