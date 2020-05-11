#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_ACTIVATION_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_ACTIVATION_H_

#include <cstddef>
#include <memory>
#include <vector>

#include "google/protobuf/field_mask.pb.h"
#include "google/protobuf/util/field_mask_util.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_value.h"
#include "eval/public/cel_value_producer.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Base class for an activation.
class BaseActivation {
 public:
  BaseActivation() = default;

  // Non-copyable/non-assignable
  BaseActivation(const BaseActivation&) = delete;
  BaseActivation& operator=(const BaseActivation&) = delete;

  // Return a list of function overloads for the given name.
  virtual std::vector<const CelFunction*> FindFunctionOverloads(
      absl::string_view) const = 0;

  // Provide the value that is bound to the name, if found.
  // arena parameter is provided to support the case when we want to pass the
  // ownership of returned object ( Message/List/Map ) to Evaluator.
  virtual absl::optional<CelValue> FindValue(absl::string_view,
                                             google::protobuf::Arena*) const = 0;

  // Check whether a select path is unknown.
  virtual bool IsPathUnknown(absl::string_view) const { return false; }

  // Return FieldMask defining the list of unknown paths.
  virtual const google::protobuf::FieldMask& unknown_paths() const {
    return google::protobuf::FieldMask::default_instance();
  }

  // Return the collection of attribute patterns that determine "unknown"
  // values.
  virtual const std::vector<CelAttributePattern>& unknown_attribute_patterns()
      const {
    static const std::vector<CelAttributePattern> empty;
    return empty;
  }

  virtual ~BaseActivation() {}
};

// Instance of Activation class is used by evaluator.
// It provides binding between references used in expressions
// and actual values.
class Activation : public BaseActivation {
 public:
  Activation() = default;

  // Non-copyable/non-assignable
  Activation(const Activation&) = delete;
  Activation& operator=(const Activation&) = delete;

  // BaseActivation
  std::vector<const CelFunction*> FindFunctionOverloads(
      absl::string_view name) const override;

  absl::optional<CelValue> FindValue(absl::string_view name,
                                     google::protobuf::Arena* arena) const override;

  bool IsPathUnknown(absl::string_view path) const override {
    return google::protobuf::util::FieldMaskUtil::IsPathInFieldMask(std::string(path),
                                                          unknown_paths_);
  }

  // Insert a function into the activation (ie a lazily bound function). Returns
  // a status if the name and shape of the function matches another one that has
  // already been bound.
  absl::Status InsertFunction(std::unique_ptr<CelFunction> function);

  // Insert value into Activation.
  void InsertValue(absl::string_view name, const CelValue& value);

  // Insert ValueProducer into Activation.
  void InsertValueProducer(absl::string_view name,
                           std::unique_ptr<CelValueProducer> value_producer);

  // Remove functions that have the same name and shape as descriptor. Returns
  // true if matching functions were found and removed.
  bool RemoveFunctionEntries(const CelFunctionDescriptor& descriptor);

  // Removes value or producer, returns true if entry with the name was found
  bool RemoveValueEntry(absl::string_view name);

  // Clears a cached value for a value producer, returns if true if entry was
  // found and cleared.
  bool ClearValueEntry(absl::string_view name);

  // Clears all cached values for value producers. Returns the number of entries
  // cleared.
  int ClearCachedValues();

  // Set unknown value paths through FieldMask
  void set_unknown_paths(google::protobuf::FieldMask mask) {
    unknown_paths_ = std::move(mask);
  }

  // Return FieldMask defining the list of unknown paths.
  const google::protobuf::FieldMask& unknown_paths() const override {
    return unknown_paths_;
  }

  // Sets the collection of attribute patterns that will be recognized as
  // "unknown" values during expression evaluation.
  void set_unknown_attribute_patterns(
      std::vector<CelAttributePattern> unknown_attribute_patterns) {
    unknown_attribute_patterns_ = std::move(unknown_attribute_patterns);
  }

  // Return the collection of attribute patterns that determine "unknown"
  // values.
  const std::vector<CelAttributePattern>& unknown_attribute_patterns()
      const override {
    return unknown_attribute_patterns_;
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

    bool ClearValue() {
      bool result = value_.has_value();
      value_.reset();
      return result;
    }

    bool HasProducer() const { return producer_ != nullptr; }

   private:
    mutable absl::optional<CelValue> value_;
    std::unique_ptr<CelValueProducer> producer_;
  };

  absl::flat_hash_map<std::string, ValueEntry> value_map_;
  absl::flat_hash_map<std::string, std::vector<std::unique_ptr<CelFunction>>>
      function_map_;

  // TODO(issues/41) deprecate when unknowns support is done.
  google::protobuf::FieldMask unknown_paths_;
  std::vector<CelAttributePattern> unknown_attribute_patterns_;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_ACTIVATION_H_
