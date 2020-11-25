#include "eval/public/testing/debug_string.h"

#include "google/protobuf/message.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/substitute.h"
#include "absl/time/time.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/unknown_function_result_set.h"
#include "eval/public/unknown_set.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {
namespace test {

namespace {

// Forward declare -- depends on value string visitor.
std::string AttributeString(const CelAttribute* attr);

std::string FunctionCallString(const UnknownFunctionResult* fn);

struct ValueStringVisitor {
  std::string operator()(int64_t arg) { return absl::StrFormat("%d", arg); }

  std::string operator()(uint64_t arg) { return absl::StrFormat("%d", arg); }

  std::string operator()(bool arg) { return (arg) ? "true" : "false"; }

  std::string operator()(double arg) { return absl::StrFormat("%f", arg); }

  std::string operator()(const google::protobuf::Message* arg) {
    if (arg == nullptr) {
      return "NULL";
    }
    return arg->DebugString();
  }
  std::string operator()(CelValue::StringHolder arg) {
    return absl::StrFormat("'%s'", arg.value());
  }

  std::string operator()(CelValue::BytesHolder arg) {
    return absl::StrFormat("0x%s", absl::BytesToHexString(arg.value()));
  }

  std::string operator()(absl::Time arg) { return absl::FormatTime(arg); }

  std::string operator()(absl::Duration arg) {
    return absl::FormatDuration(arg);
  }

  std::string operator()(const CelList* arg) {
    std::vector<std::string> elements;
    elements.reserve(arg->size());
    for (int i = 0; i < arg->size(); i++) {
      elements.push_back(DebugString(arg->operator[](i)));
    }
    return absl::StrCat("[", absl::StrJoin(elements, ", "), "]");
  }

  std::string operator()(const CelMap* arg) {
    const CelList* keys = arg->ListKeys();
    std::vector<std::string> elements;
    elements.reserve(keys->size());
    for (int i = 0; i < keys->size(); i++) {
      elements.push_back(
          absl::Substitute("$0:$1", DebugString((*keys)[i]),
                           DebugString(arg->operator[]((*keys)[i]).value())));
    }
    return absl::Substitute("{$0}", absl::StrJoin(elements, ", "));
  }

  std::string operator()(CelValue::CelTypeHolder arg) {
    return absl::StrFormat("'%s'", arg.value());
  }

  std::string operator()(const CelError* arg) { return arg->ToString(); }

  std::string operator()(const UnknownSet* arg) {
    std::vector<std::string> attrs;
    attrs.reserve(arg->unknown_attributes().attributes().size());
    for (const auto* attr : arg->unknown_attributes().attributes()) {
      attrs.push_back(AttributeString(attr));
    }
    std::vector<std::string> fns;
    fns.reserve(
        arg->unknown_function_results().unknown_function_results().size());
    for (const auto* fn :
         arg->unknown_function_results().unknown_function_results()) {
      fns.push_back(FunctionCallString(fn));
    }
    return absl::Substitute("{attributes:[$0], functions:[$1]}",
                            absl::StrJoin(attrs, ", "),
                            absl::StrJoin(fns, ", "));
  }
};

std::string AttributeString(const CelAttribute* attr) {
  // qualification =
  std::string output(attr->variable().ident_expr().name());
  for (const auto& q : attr->qualifier_path()) {
    absl::StrAppend(&output, ".", q.Visit<std::string>(ValueStringVisitor()));
  }
  return output;
}

std::string FunctionCallString(const UnknownFunctionResult* fn) {
  std::vector<std::string> args;
  std::string call;
  args.reserve(fn->arguments().size());
  if (fn->descriptor().receiver_style()) {
    if (fn->arguments().empty()) {
      absl::StrAppend(&call, "<Missing Receiver>.");
    } else {
      absl::StrAppend(&call, DebugString(fn->arguments()[0]), ".");
    }
    absl::StrAppend(&call, fn->descriptor().name());
    for (size_t i = 1; i < fn->arguments().size(); i++) {
      args.push_back(DebugString(fn->arguments()[i]));
    }
  } else {
    absl::StrAppend(&call, fn->descriptor().name());
    for (size_t i = 0; i < fn->arguments().size(); i++) {
      args.push_back(DebugString(fn->arguments()[i]));
    }
  }
  return absl::Substitute("$0($1)", call, absl::StrJoin(args, ", "));
}

}  // namespace

// String rerpesentation of the underlying value.
std::string DebugValueString(const CelValue& value) {
  return value.Visit<std::string>(ValueStringVisitor());
}

// String representation of the cel value.
std::string DebugString(const CelValue& value) {
  return absl::Substitute("<$0,$1>", CelValue::TypeName(value.type()),
                          DebugValueString(value));
}

}  // namespace test
}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
