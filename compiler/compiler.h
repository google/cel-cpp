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

#ifndef THIRD_PARTY_CEL_CPP_COMPILER_COMPILER_INTERFACE_H_
#define THIRD_PARTY_CEL_CPP_COMPILER_COMPILER_INTERFACE_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "checker/checker_options.h"
#include "checker/type_checker_builder.h"
#include "checker/validation_result.h"
#include "parser/options.h"
#include "parser/parser_interface.h"

namespace cel {

class Compiler;
class CompilerBuilder;

// Callable for configuring a ParserBuilder.
using ParserBuilderConfigurer =
    absl::AnyInvocable<absl::Status(ParserBuilder&) const>;

// A CompilerLibrary represents a package of CEL configuration that can be
// added to a Compiler.
//
// It may contain either or both of a Parser configuration and a
// TypeChecker configuration.
struct CompilerLibrary {
  // Optional identifier to avoid collisions re-adding the same library.
  // If id is empty, it is not considered.
  std::string id;
  // Optional callback for configuring the parser.
  ParserBuilderConfigurer configure_parser;
  // Optional callback for configuring the type checker.
  TypeCheckerBuilderConfigurer configure_checker;

  CompilerLibrary(std::string id, ParserBuilderConfigurer configure_parser,
                  TypeCheckerBuilderConfigurer configure_checker = nullptr)
      : id(std::move(id)),
        configure_parser(std::move(configure_parser)),
        configure_checker(std::move(configure_checker)) {}

  CompilerLibrary(std::string id,
                  TypeCheckerBuilderConfigurer configure_checker)
      : id(std::move(id)),
        configure_parser(std::move(nullptr)),
        configure_checker(std::move(configure_checker)) {}

  // Convenience conversion from the CheckerLibrary type.
  // NOLINTNEXTLINE(google-explicit-constructor)
  CompilerLibrary(CheckerLibrary checker_library)
      : id(std::move(checker_library.id)),
        configure_parser(nullptr),
        configure_checker(std::move(checker_library.configure)) {}
};

// General options for configuring the underlying parser and checker.
struct CompilerOptions {
  ParserOptions parser_options;
  CheckerOptions checker_options;
};

// Interface for CEL CompilerBuilder objects.
//
// Builder implementations are thread hostile, but should create
// thread-compatible Compiler instances.
class CompilerBuilder {
 public:
  virtual ~CompilerBuilder() = default;

  virtual absl::Status AddLibrary(cel::CompilerLibrary library) = 0;

  virtual TypeCheckerBuilder& GetCheckerBuilder() = 0;
  virtual ParserBuilder& GetParserBuilder() = 0;

  virtual absl::StatusOr<std::unique_ptr<Compiler>> Build() && = 0;
};

// Interface for CEL Compiler objects.
//
// For CEL, compilation is the process of bundling the parse and type-check
// passes.
//
// Compiler instances should be thread-compatible.
class Compiler {
 public:
  virtual ~Compiler() = default;

  virtual absl::StatusOr<ValidationResult> Compile(
      absl::string_view source, absl::string_view description) const = 0;

  absl::StatusOr<ValidationResult> Compile(absl::string_view source) const {
    return Compile(source, "<input>");
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMPILER_COMPILER_INTERFACE_H_
