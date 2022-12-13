#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_STACK_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_STACK_H_

#include <type_traits>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "base/handle.h"
#include "base/value.h"
#include "eval/eval/attribute_trail.h"
#include "eval/internal/interop.h"

namespace google::api::expr::runtime {

// CelValue stack.
// Implementation is based on vector to allow passing parameters from
// stack as Span<>.
class EvaluatorStack {
 public:
  explicit EvaluatorStack(size_t max_size)
      : max_size_(max_size), current_size_(0) {
    Reserve(max_size);
  }

  // Return the current stack size.
  size_t size() const { return current_size_; }

  // Return the maximum size of the stack.
  size_t max_size() const { return max_size_; }

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
  absl::Span<const cel::Handle<cel::Value>> GetSpan(size_t size) const {
    if (!HasEnough(size)) {
      LOG(ERROR) << "Requested span size (" << size
                 << ") exceeds current stack size: " << current_size_;
    }
    return absl::Span<const cel::Handle<cel::Value>>(
        stack_.data() + current_size_ - size, size);
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
  const cel::Handle<cel::Value>& Peek() const {
    if (empty()) {
      LOG(ERROR) << "Peeking on empty EvaluatorStack";
    }
    return stack_[current_size_ - 1];
  }

  // Peeks the last element of the attribute stack.
  // Checking that stack is not empty is caller's responsibility.
  const AttributeTrail& PeekAttribute() const {
    if (empty()) {
      LOG(ERROR) << "Peeking on empty EvaluatorStack";
    }
    return attribute_stack_[current_size_ - 1];
  }

  // Clears the last size elements of the stack.
  // Checking that stack has enough elements is caller's responsibility.
  void Pop(size_t size) {
    if (!HasEnough(size)) {
      LOG(ERROR) << "Trying to pop more elements (" << size
                 << ") than the current stack size: " << current_size_;
    }
    while (size > 0) {
      stack_.pop_back();
      attribute_stack_.pop_back();
      current_size_--;
      size--;
    }
  }

  // Put element on the top of the stack.
  void Push(cel::Handle<cel::Value> value) {
    Push(std::move(value), AttributeTrail());
  }

  void Push(cel::Handle<cel::Value> value, AttributeTrail attribute) {
    if (current_size_ >= max_size()) {
      LOG(ERROR) << "No room to push more elements on to EvaluatorStack";
    }
    stack_.push_back(std::move(value));
    attribute_stack_.push_back(std::move(attribute));
    current_size_++;
  }

  // Replace element on the top of the stack.
  // Checking that stack is not empty is caller's responsibility.
  void PopAndPush(cel::Handle<cel::Value> value) {
    PopAndPush(std::move(value), AttributeTrail());
  }

  // Replace element on the top of the stack.
  // Checking that stack is not empty is caller's responsibility.
  void PopAndPush(cel::Handle<cel::Value> value, AttributeTrail attribute) {
    if (empty()) {
      LOG(ERROR) << "Cannot PopAndPush on empty stack.";
    }
    stack_[current_size_ - 1] = std::move(value);
    attribute_stack_[current_size_ - 1] = std::move(attribute);
  }

  // Preallocate stack.
  void Reserve(size_t size) {
    if (size > max_size()) {
      size = max_size();
    }
    stack_.reserve(size);
    attribute_stack_.reserve(size);
  }

 private:
  std::vector<cel::Handle<cel::Value>> stack_;
  std::vector<AttributeTrail> attribute_stack_;
  const size_t max_size_;
  size_t current_size_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_STACK_H_
