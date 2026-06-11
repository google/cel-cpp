// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "internal/status_macros.h"
#include "policy/cel_policy.h"
#include "policy/cel_policy_parse_context.h"
#include "policy/cel_policy_parser.h"
#include "policy/yaml_policy_parser.h"
#include "yaml-cpp/node/node.h"
#include "yaml-cpp/yaml.h"  // IWYU pragma: keep

namespace cel::internal {

// TestCustomYamlPolicyParser is used to support unit tests for custom tags
// and custom policy structures. It demonstrates the versatility of the
// cel::YamlPolicyParser framework API by implementing custom tag and block
// parsing without needing to modify the core parser.
class TestCustomYamlPolicyParser : public cel::YamlPolicyParser {
  absl::StatusOr<bool> ParsePolicyTag(CelPolicyParseContext& ctx,
                                      const ValueString& tag_name,
                                      const YAML::Node& node) const override {
    if (tag_name.value() == "name" || tag_name.value() == "description" ||
        tag_name.value() == "imports") {
      return cel::YamlPolicyParser::ParsePolicyTag(ctx, tag_name, node);
    }
    if (tag_name.value() == "purpose") {
      std::optional<ValueString> purpose =
          GetValueString(ctx, node, "Policy purpose is not a string");
      if (purpose.has_value()) {
        ctx.policy().mutable_metadata()["purpose"] = *purpose;
      }
      return true;
    }
    if (tag_name.value() == "version") {
      std::optional<ValueString> version =
          GetValueString(ctx, node, "Policy version is not a string");
      if (!version.has_value()) {
        return true;
      }
      int version_int;
      if (!absl::SimpleAtoi(version->value(), &version_int)) {
        ctx.ReportError(version->id(),
                        absl::StrCat("Policy version is not an integer: ",
                                     version->value()));
        return true;
      }
      ctx.policy().mutable_metadata()["version"] = version_int;
      return true;
    }

    if (tag_name.value() == "conditions") {
      if (!node.IsSequence()) {
        ctx.ReportError(tag_name.id(), "Policy 'conditions' is not a sequence");
        return true;
      }
      for (const YAML::Node& condition : node) {
        // Track the number of existing matches before parsing. When ParseMatch
        // evaluates an 'else' block, it recursively triggers parsing and adds
        // internal inner matches directly to the rule's match vector.
        // Inserting the outer match at begin() + size_before ensures that the
        // primary outer 'if' condition is always evaluated before its nested
        // 'else' fallbacks.
        //
        // Example:
        // if: x > 0
        // then: "positive"
        // else: "negative"
        //
        // The inner "negative" match is parsed and appended to rule.matches()
        // by the inner recursive call, before the outer "x > 0" match finishes.
        // Inserting at size_before places the "x > 0" match ahead of the inner
        // one.
        size_t size_before = ctx.policy().rule().matches().size();
        CEL_ASSIGN_OR_RETURN(Match match,
                             cel::YamlPolicyParser::ParseMatch(
                                 ctx, condition, ctx.policy().mutable_rule()));
        ctx.policy().mutable_rule().mutable_matches().insert(
            ctx.policy().mutable_rule().mutable_matches().begin() + size_before,
            std::move(match));
      }

      return true;
    }
    return false;
  }

  absl::Status ParseThenBlock(CelPolicyParseContext& ctx,
                              const YAML::Node& value_node,
                              Match& match) const {
    if (value_node.IsScalar()) {
      std::optional<ValueString> val = GetValueString(
          ctx, value_node, "Policy condition 'then' is not a string");
      if (val.has_value()) {
        OutputBlock output;
        output.set_output(*val);
        match.set_result(output);
      }
    } else if (value_node.IsMap()) {
      auto nested_rule = std::make_unique<Rule>();
      CEL_ASSIGN_OR_RETURN(
          Match nested_match,
          cel::YamlPolicyParser::ParseMatch(ctx, value_node, *nested_rule));
      nested_rule->mutable_matches().insert(
          nested_rule->mutable_matches().begin(), std::move(nested_match));
      match.set_result(std::move(nested_rule));
    } else {
      ctx.ReportError(CollectMetadata(ctx, value_node),
                      "Bad syntax in 'if/then' block");
    }
    return absl::OkStatus();
  }

  absl::Status ParseElseBlock(CelPolicyParseContext& ctx,
                              const YAML::Node& value_node, Rule& rule) const {
    if (value_node.IsScalar()) {
      std::optional<ValueString> val = GetValueString(
          ctx, value_node, "Policy condition 'else' is not a string");
      if (val.has_value()) {
        Match else_match;
        else_match.set_id(CollectMetadata(ctx, value_node));
        OutputBlock output;
        output.set_output(*val);
        else_match.set_result(output);
        rule.mutable_matches().push_back(std::move(else_match));
      }
    } else if (value_node.IsMap()) {
      size_t size_before = rule.matches().size();
      CEL_ASSIGN_OR_RETURN(Match match, cel::YamlPolicyParser::ParseMatch(
                                            ctx, value_node, rule));
      rule.mutable_matches().insert(
          rule.mutable_matches().begin() + size_before, std::move(match));
    } else {
      ctx.ReportError(CollectMetadata(ctx, value_node),
                      "Bad syntax in 'if/then' block");
    }
    return absl::OkStatus();
  }

  absl::StatusOr<bool> ParseMatchTag(CelPolicyParseContext& ctx,
                                     const ValueString& tag_name,
                                     const YAML::Node& node, Match& match,
                                     Rule& rule) const override {
    if (tag_name.value() == "if") {
      std::optional<ValueString> condition =
          GetValueString(ctx, node, "Policy 'if' condition is not a string");
      if (condition.has_value()) {
        match.set_condition(*condition);
      }
      return true;
    }
    if (tag_name.value() == "then") {
      CEL_RETURN_IF_ERROR(ParseThenBlock(ctx, node, match));
      return true;
    }
    if (tag_name.value() == "else") {
      CEL_RETURN_IF_ERROR(ParseElseBlock(ctx, node, rule));
      return true;
    }
    return false;
  }
};

const CelPolicyParser<YAML::Node>& GetTestCustomYamlPolicyParser() {
  static const auto* const parser = new TestCustomYamlPolicyParser();
  return *parser;
}

}  // namespace cel::internal
