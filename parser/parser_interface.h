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
#ifndef THIRD_PARTY_CEL_CPP_PARSER_PARSER_INTERFACE_H_
#define THIRD_PARTY_CEL_CPP_PARSER_PARSER_INTERFACE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/ast.h"
#include "common/source.h"
#include "parser/macro.h"
#include "parser/options.h"

namespace cel {

class Parser;
class ParserBuilder;

// Callable for configuring a ParserBuilder.
using ParserBuilderConfigurer =
    absl::AnyInvocable<absl::Status(ParserBuilder&) const>;

struct ParserLibrary {
  // Optional identifier to avoid collisions re-adding the same macros. If
  // empty, it is not considered for collision detection.
  std::string id;
  ParserBuilderConfigurer configure;
};

// Declares a subset of a parser library.
struct ParserLibrarySubset {
  // The id of the library to subset. Only one subset can be applied per
  // library id.
  //
  // Must be non-empty.
  std::string library_id;

  using MacroPredicate = absl::AnyInvocable<bool(const Macro&) const>;
  MacroPredicate should_include_macro;
};

// Interface for building a CEL parser, see comments on `Parser` below.
class ParserBuilder {
 public:
  virtual ~ParserBuilder() = default;

  // Returns the (mutable) current parser options.
  virtual ParserOptions& GetOptions() = 0;

  // Adds a macro to the parser.
  // Standard macros should be automatically added based on parser options.
  virtual absl::Status AddMacro(const cel::Macro& macro) = 0;

  virtual absl::Status AddLibrary(ParserLibrary library) = 0;

  virtual absl::Status AddLibrarySubset(ParserLibrarySubset subset) = 0;

  // Builds a new parser instance, may error if incompatible macros are added.
  virtual absl::StatusOr<std::unique_ptr<Parser>> Build() = 0;
};

// Information about a parse failure.
class ParseIssue {
 public:
  explicit ParseIssue(std::string message) : message_(std::move(message)) {}
  ParseIssue(SourceLocation location, std::string message)
      : location_(location), message_(std::move(message)) {}

  ParseIssue(const ParseIssue& other) = default;
  ParseIssue& operator=(const ParseIssue& other) = default;
  ParseIssue(ParseIssue&& other) = default;
  ParseIssue& operator=(ParseIssue&& other) = default;

  SourceLocation location() const { return location_; }
  absl::string_view message() const { return message_; }

 private:
  SourceLocation location_;
  std::string message_;
};

// Interface for stateful CEL parser objects for use with a `Compiler`
// (bundled parse and type check). This is not needed for most users:
// prefer using the free functions in `parser.h` for more flexibility.
class Parser {
 public:
  virtual ~Parser() = default;

  // Parses the given source into a CEL AST.
  absl::StatusOr<std::unique_ptr<cel::Ast>> Parse(
      const cel::Source& source) const;

  // Parses the given source into a CEL AST, collecting parse errors in
  // `issues`. If `issues` is non-null, it will be cleared and all parse
  // issues will be appended to it.
  absl::StatusOr<std::unique_ptr<cel::Ast>> Parse(
      const cel::Source& source, std::vector<ParseIssue>* issues) const;

  // Returns a builder initialized with the configuration of this parser.
  virtual std::unique_ptr<ParserBuilder> ToBuilder() const = 0;

 protected:
  virtual absl::StatusOr<std::unique_ptr<cel::Ast>> ParseImpl(
      const cel::Source& source,
      std::vector<ParseIssue>* absl_nullable parse_issues) const = 0;
};

inline absl::StatusOr<std::unique_ptr<cel::Ast>> Parser::Parse(
    const cel::Source& source) const {
  return ParseImpl(source, nullptr);
}

inline absl::StatusOr<std::unique_ptr<cel::Ast>> Parser::Parse(
    const cel::Source& source, std::vector<ParseIssue>* issues) const {
  if (issues != nullptr) issues->clear();
  return ParseImpl(source, issues);
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_PARSER_PARSER_INTERFACE_H_
