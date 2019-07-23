#include "common/error.h"
#include "google/rpc/code.pb.h"
#include "internal/cel_printer.h"
#include "internal/hash_util.h"

namespace google {
namespace api {
namespace expr {
namespace common {

Error::Error(const google::rpc::Status& error) { errors_.insert(error); }

Error::Error(absl::Span<const google::rpc::Status* const> errors) {
  for (const auto* error : errors) {
    errors_.insert(*error);
  }
}

Error::Error(absl::Span<const google::rpc::Status> errors) {
  for (const auto& error : errors) {
    errors_.insert(error);
  }
}

std::size_t Error::hash_code() const {
  std::size_t code = internal::kIntegralTypeOffset;
  for (const auto& error : errors_) {
    code = internal::MixHashNoOrder(
        internal::Hash(error.code(), error.message()), code);
  }
  return code;
}

const Error::ErrorData& Error::errors() const { return errors_; }

bool Error::operator==(const Error& rhs) const {
  if (this == &rhs) {
    return true;
  }
  if (hash_code() != rhs.hash_code() || errors_.size() != rhs.errors_.size()) {
    return false;
  }
  for (const auto& error : errors_) {
    if (rhs.errors_.find(error) == rhs.errors_.end()) {
      return false;
    }
  }
  return true;
}

std::string Error::ToDebugString() const {
  std::multiset<absl::string_view> codes;
  for (const auto& error : errors_) {
    codes.emplace(
        google::rpc::Code_Name(static_cast<google::rpc::Code>(error.code())));
  }

  internal::VarSequencePrinter<internal::SetJoinPolicy> printer;
  return printer("Error", internal::RawString{absl::StrJoin(
                              codes, internal::SetJoinPolicy::kValueDelim)});
}

}  // namespace common
}  // namespace expr
}  // namespace api
}  // namespace google
