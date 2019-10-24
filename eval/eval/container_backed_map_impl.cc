

#include "eval/eval/container_backed_map_impl.h"
#include "absl/container/node_hash_map.h"
#include "absl/types/span.h"
#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

// Helper classes for CelValue hasher function
// We care only for hash operations for integral/string types, but others
// should be present as well for CelValue::Visit(HasherOp) to compile.
class HasherOp {
 public:
  template <class T>
  size_t operator()(const T& arg) {
    return std::hash<T>()(arg);
  }

  size_t operator()(const absl::Time arg) {
    return absl::Hash<absl::Time>()(arg);
  }

  size_t operator()(const absl::Duration arg) {
    return absl::Hash<absl::Duration>()(arg);
  }

  size_t operator()(const CelValue::StringHolder& arg) {
    return absl::Hash<absl::string_view>()(arg.value());
  }

  size_t operator()(const CelValue::BytesHolder& arg) {
    return absl::Hash<absl::string_view>()(arg.value());
  }

  // Needed for successful compilation resolution.
  size_t operator()(const std::nullptr_t&) { return 0; }
};

// Helper classes to provide CelValue equality comparison operation
template <class T>
class EqualOp {
 public:
  explicit EqualOp(const T& arg) : arg_(arg) {}

  template <class U>
  bool operator()(const U&) const {
    return false;
  }

  bool operator()(const T& other) const { return other == arg_; }

 private:
  const T& arg_;
};

class CelValueEq {
 public:
  explicit CelValueEq(const CelValue& other) : other_(other) {}

  template <class Type>
  bool operator()(const Type& arg) {
    return other_.template Visit<bool>(EqualOp<Type>(arg));
  }

 private:
  const CelValue& other_;
};

// CelValue hasher functor.
class Hasher {
 public:
  size_t operator()(const CelValue& key) const {
    return key.template Visit<size_t>(HasherOp());
  }
};

// CelValue equality functor.
class Equal {
 public:
  //
  bool operator()(const CelValue& key1, const CelValue& key2) const {
    if (key1.type() != key2.type()) {
      return false;
    }
    return key1.template Visit<bool>(CelValueEq(key2));
  }
};

// CelMap implementation that uses STL map container as backing storage.
// KeyType is the type of key values stored in CelValue, InnerKeyType is the
// type of key in STL map.
class ContainerBackedMapImpl : public CelMap {
 public:
  static std::unique_ptr<CelMap> Create(
      absl::Span<std::pair<CelValue, CelValue>> key_values) {
    auto cel_map = absl::WrapUnique(new ContainerBackedMapImpl());

    if (!cel_map->AddItems(key_values)) {
      return nullptr;
    }
    return std::move(cel_map);
  }

  // Map size.
  int size() const override { return values_map_.size(); }

  // Map element access operator.
  absl::optional<CelValue> operator[](CelValue cel_key) const override {
    auto item = values_map_.find(cel_key);
    if (item == values_map_.end()) {
      return {};
    }
    return item->second;
  }

  const CelList* ListKeys() const override { return &key_list_; }

 private:
  class KeyList : public CelList {
   public:
    int size() const override { return keys_.size(); }

    CelValue operator[](int index) const override { return keys_[index]; }

    void Add(const CelValue& key) { keys_.push_back(key); }

   private:
    std::vector<CelValue> keys_;
  };

  ContainerBackedMapImpl() = default;

  bool AddItems(absl::Span<std::pair<CelValue, CelValue>> key_values) {
    for (const auto& item : key_values) {
      auto result = values_map_.emplace(item.first, item.second);

      // Failed to insert pair into map - addition failed.
      if (!result.second) {
        return false;
      }
      key_list_.Add(item.first);
    }
    return true;
  }

  absl::node_hash_map<CelValue, CelValue, Hasher, Equal> values_map_;
  KeyList key_list_;
};

}  // namespace

std::unique_ptr<CelMap> CreateContainerBackedMap(
    absl::Span<std::pair<CelValue, CelValue>> key_values) {
  return ContainerBackedMapImpl::Create(key_values);
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
