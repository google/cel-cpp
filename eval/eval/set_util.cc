#include "eval/eval/set_util.h"

#include <algorithm>

namespace google {
namespace api {
namespace expr {
namespace runtime {
namespace {

// Default implementation is operator<.
// Note: for UnknownSet, Error and Message, this is ptr less than.
template <typename T>
bool LessThanImpl(T lhs, T rhs) {
  return lhs < rhs;
}

// List specialization -- compare size then elementwise compare.
template <>
bool LessThanImpl(const CelList* lhs, const CelList* rhs) {
  if (lhs->size() < rhs->size()) {
    return true;
  } else if (lhs->size() > rhs->size()) {
    return false;
  }
  for (int i = 0; i < lhs->size(); i++) {
    CelValue lhs_i = lhs->operator[](i);
    CelValue rhs_i = rhs->operator[](i);
    if (CelValueLessThan(lhs_i, rhs_i)) {
      return true;
    }
    if (CelValueLessThan(rhs_i, lhs_i)) {
      return false;
    }
  }
  // equal
  return false;
}

// Map specialization -- size then sorted elementwise compare (i.e.
// <lhs_key_i, lhs_value_i> < <rhs_key_i, rhs_value_i>
//
// This is expensive, but hopefully maps will be rarely used in sets.
template <>
bool LessThanImpl(const CelMap* lhs, const CelMap* rhs) {
  if (lhs->size() < rhs->size()) {
    return true;
  } else if (lhs->size() > rhs->size()) {
    return false;
  }

  std::vector<CelValue> lhs_keys;
  std::vector<CelValue> rhs_keys;
  lhs_keys.reserve(lhs->size());
  rhs_keys.reserve(lhs->size());

  const CelList* lhs_key_view = lhs->ListKeys();
  const CelList* rhs_key_view = rhs->ListKeys();

  for (int i = 0; i < lhs->size(); i++) {
    lhs_keys.push_back(lhs_key_view->operator[](i));
    rhs_keys.push_back(rhs_key_view->operator[](i));
  }

  std::sort(lhs_keys.begin(), lhs_keys.end(), &CelValueLessThan);
  std::sort(rhs_keys.begin(), rhs_keys.end(), &CelValueLessThan);

  for (int i = 0; i < lhs_keys.size(); i++) {
    auto lhs_key_i = lhs_keys[i];
    auto rhs_key_i = rhs_keys[i];
    if (CelValueLessThan(lhs_key_i, rhs_key_i)) {
      return true;
    }
    if (CelValueLessThan(rhs_key_i, lhs_key_i)) {
      return false;
    }
    // keys equal, compare values.
    auto lhs_value_i = lhs->operator[](lhs_key_i).value();
    auto rhs_value_i = rhs->operator[](rhs_key_i).value();

    if (CelValueLessThan(lhs_value_i, rhs_value_i)) {
      return true;
    }
    if (CelValueLessThan(rhs_value_i, lhs_value_i)) {
      return true;
    }
  }
  // maps equal
  return false;
}

struct LessThanVisitor {
  CelValue rhs;
  LessThanVisitor(CelValue rhs) : rhs(rhs) {}
  template <typename T>
  bool operator()(T lhs_value) {
    T rhs_value;
    if (!rhs.GetValue(&rhs_value)) {
      return CelValue::Type(CelValue::IndexOf<T>::value) < rhs.type();
    }
    return LessThanImpl(lhs_value, rhs_value);
  }
};

}  // namespace

bool CelValueLessThan(CelValue lhs, CelValue rhs) {
  return lhs.Visit<bool>(LessThanVisitor(rhs));
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
