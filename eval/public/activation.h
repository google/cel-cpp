#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_ACTIVATION_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_ACTIVATION_H_

#include "google/protobuf/field_mask.pb.h"
#include "google/protobuf/util/field_mask_util.h"
#include "absl/strings/string_view.h"
#include "eval/public/cel_value.h"
#include "eval/public/cel_value_producer.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Instance of Activation class is used by evaluator.
// It provides binding between references used in expressions
// and actual values.
class Activation {
 public:
  Activation() = default;

  // Non-copyable/non-assignable
  Activation(const Activation&) = delete;
  Activation& operator=(const Activation&) = delete;

  // Provide value that is bound to the name, if found.
  // arena parameter is provided to support the case when we want to pass the
  // ownership of returned object ( Message/List/Map ) to Evaluator.
  absl::optional<CelValue> FindValue(absl::string_view name,
                                     google::protobuf::Arena* arena) const;

  // Insert value into Activation.
  void InsertValue(absl::string_view name, const CelValue& value);

  // Insert ValueProducer into Activation.
  void InsertValueProducer(absl::string_view name,
                           std::unique_ptr<CelValueProducer> value_producer);

  // Removes value or producer, returns true if entry with the name was found
  bool RemoveValueEntry(absl::string_view name);

  // Set unknown value paths through FieldMask
  void set_unknown_paths(google::protobuf::FieldMask mask) {
    unknown_paths_ = std::move(mask);
  }

  // Return FieldMask defining the list of unknown paths.
  const google::protobuf::FieldMask unknown_paths() const {
    return unknown_paths_;
  }

  // Check whether select path is unknown.
  bool IsPathUnknown(absl::string_view path) const {
    return google::protobuf::util::FieldMaskUtil::IsPathInFieldMask(std::string(path),
                                                          unknown_paths_);
  }

 private:
  class ValueEntry {
   public:
    explicit ValueEntry(std::unique_ptr<CelValueProducer> prod)
        : value_(), producer_(std::move(prod)) {}

    explicit ValueEntry(const CelValue& value) : value_(value), producer_() {}

    // Retrieve associated CelValue.
    // If the value is not set and producer is set,
    // obtain and cache value from producer.
    absl::optional<CelValue> RetrieveValue(google::protobuf::Arena* arena) const {
      if (!value_.has_value()) {
        if (producer_) {
          value_ = producer_->Produce(arena);
        }
      }

      return value_;
    }

   private:
    mutable absl::optional<CelValue> value_;
    std::unique_ptr<CelValueProducer> producer_;
  };

  std::map<std::string, ValueEntry> value_map_;

  google::protobuf::FieldMask unknown_paths_;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_ACTIVATION_H_
