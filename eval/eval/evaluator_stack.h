#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_STACK_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_STACK_H_

#include <vector>

#include "absl/types/span.h"
#include "eval/eval/attribute_trail.h"
#include "eval/public/cel_value.h"

namespace google::api::expr::runtime {

// CelValue stack.
// Implementation is based on vector to allow passing parameters from
// stack as Span<>.
class EvaluatorStack {
 public:
  explicit EvaluatorStack(size_t max_size) : current_size_(0) {
    stack_.resize(max_size);
    attribute_stack_.resize(max_size);
  }

  // Return the current stack size.
  size_t size() const { return current_size_; }

  // Return the maximum size of the stack.
  size_t max_size() const { return stack_.size(); }

  // Returns true if stack is empty.
  bool empty() const { return current_size_ == 0; }

  // Attributes stack size.
  size_t attribute_size() const { return current_size_; }

  // Check that stack has enough elements.
  bool HasEnough(size_t size) const { return current_size_ >= size; }

  // Dumps the entire stack state as is.
  void Clear();

  // Gets the last size elements of the stack.
  // Checking that stack has enough elements is caller's responsibility.
  // Please note that calls to Push may invalidate returned Span object.
  absl::Span<const CelValue> GetSpan(size_t size) const {
    if (!HasEnough(size)) {
      GOOGLE_LOG(ERROR) << "Requested span size (" << size
                 << ") exceeds current stack size: " << current_size_;
    }
    return absl::Span<const CelValue>(stack_.data() + current_size_ - size,
                                      size);
  }

  // Gets the last size attribute trails of the stack.
  // Checking that stack has enough elements is caller's responsibility.
  // Please note that calls to Push may invalidate returned Span object.
  absl::Span<const AttributeTrail> GetAttributeSpan(size_t size) const {
    return absl::Span<const AttributeTrail>(
        attribute_stack_.data() + current_size_ - size, size);
  }

  // Peeks the last element of the stack.
  // Checking that stack is not empty is caller's responsibility.
  const CelValue& Peek() const {
    if (empty()) {
      GOOGLE_LOG(ERROR) << "Peeking on empty EvaluatorStack";
    }
    return stack_[current_size_ - 1];
  }

  // Peeks the last element of the attribute stack.
  // Checking that stack is not empty is caller's responsibility.
  const AttributeTrail& PeekAttribute() const {
    if (empty()) {
      GOOGLE_LOG(ERROR) << "Peeking on empty EvaluatorStack";
    }
    return attribute_stack_[current_size_ - 1];
  }

  // Clears the last size elements of the stack.
  // Checking that stack has enough elements is caller's responsibility.
  void Pop(size_t size) {
    if (!HasEnough(size)) {
      GOOGLE_LOG(ERROR) << "Trying to pop more elements (" << size
                 << ") than the current stack size: " << current_size_;
    }
    current_size_ -= size;
  }

  // Put element on the top of the stack.
  void Push(const CelValue& value) { Push(value, AttributeTrail()); }

  void Push(const CelValue& value, AttributeTrail attribute) {
    if (current_size_ >= stack_.size()) {
      GOOGLE_LOG(ERROR) << "No room to push more elements on to EvaluatorStack";
    }
    stack_[current_size_] = value;
    attribute_stack_[current_size_] = attribute;
    current_size_++;
  }

  // Replace element on the top of the stack.
  // Checking that stack is not empty is caller's responsibility.
  void PopAndPush(const CelValue& value) {
    PopAndPush(value, AttributeTrail());
  }

  // Replace element on the top of the stack.
  // Checking that stack is not empty is caller's responsibility.
  void PopAndPush(const CelValue& value, AttributeTrail attribute) {
    if (empty()) {
      GOOGLE_LOG(ERROR) << "Cannot PopAndPush on empty stack.";
    }
    stack_[current_size_ - 1] = value;
    attribute_stack_[current_size_ - 1] = attribute;
  }

  // Preallocate stack.
  void Reserve(size_t size) {
    stack_.reserve(size);
    attribute_stack_.reserve(size);
  }

  // If overload resolution fails and some arguments are null, try coercing
  // to message type nullptr.
  // Returns true if any values are successfully converted.
  bool CoerceNullValues(size_t size) {
    if (!HasEnough(size)) {
      GOOGLE_LOG(ERROR) << "Trying to coerce more elements (" << size
                 << ") than the current stack size: " << current_size_;
    }
    bool updated = false;
    for (size_t i = current_size_ - size; i < stack_.size(); i++) {
      if (stack_[i].IsNull()) {
        stack_[i] = CelValue::CreateNullMessage();
        updated = true;
      }
    }
    return updated;
  }

 private:
  std::vector<CelValue> stack_;
  std::vector<AttributeTrail> attribute_stack_;
  size_t current_size_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_STACK_H_
