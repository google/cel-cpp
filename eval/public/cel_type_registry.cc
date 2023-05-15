#include "eval/public/cel_type_registry.h"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "google/protobuf/struct.pb.h"
#include "google/protobuf/descriptor.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "base/handle.h"
#include "base/memory.h"
#include "base/type_factory.h"
#include "base/types/enum_type.h"
#include "base/value.h"
#include "eval/internal/interop.h"
#include "internal/no_destructor.h"

namespace google::api::expr::runtime {

namespace {

using cel::Handle;
using cel::MemoryManager;
using cel::TypeFactory;
using cel::UniqueRef;
using cel::Value;
using cel::interop_internal::CreateTypeValueFromView;

const absl::node_hash_set<std::string>& GetCoreTypes() {
  static const auto* const kCoreTypes =
      new absl::node_hash_set<std::string>{{"bool"},
                                           {"bytes"},
                                           {"double"},
                                           {"google.protobuf.Duration"},
                                           {"google.protobuf.Timestamp"},
                                           {"int"},
                                           {"list"},
                                           {"map"},
                                           {"null_type"},
                                           {"string"},
                                           {"type"},
                                           {"uint"}};
  return *kCoreTypes;
}

using DescriptorSet = absl::flat_hash_set<const google::protobuf::EnumDescriptor*>;
using EnumMap = absl::flat_hash_map<std::string, cel::Handle<cel::EnumType>>;

// Type factory for ref-counted type instances.
cel::TypeFactory& GetDefaultTypeFactory() {
  static TypeFactory* factory = new TypeFactory(cel::MemoryManager::Global());
  return *factory;
}

// EnumType implementation for generic enums that are defined at runtime that
// can be resolved in expressions.
//
// Note: this implementation is primarily used for inspecting the full set of
// enum constants rather than looking up constants by name or number.
class ResolveableEnumType final : public cel::EnumType {
 public:
  using Constant = EnumType::Constant;
  using Enumerator = CelTypeRegistry::Enumerator;

  ResolveableEnumType(std::string name, std::vector<Enumerator> enumerators)
      : name_(std::move(name)), enumerators_(std::move(enumerators)) {}

  static const ResolveableEnumType& Cast(const Type& type) {
    ABSL_ASSERT(Is(type));
    return static_cast<const ResolveableEnumType&>(type);
  }

  absl::string_view name() const override { return name_; }

  size_t constant_count() const override { return enumerators_.size(); };

  absl::StatusOr<UniqueRef<ConstantIterator>> NewConstantIterator(
      MemoryManager& memory_manager) const override {
    return cel::MakeUnique<Iterator>(memory_manager, enumerators_);
  }

  const std::vector<Enumerator>& enumerators() const { return enumerators_; }

  absl::StatusOr<absl::optional<Constant>> FindConstantByName(
      absl::string_view name) const override;

  absl::StatusOr<absl::optional<Constant>> FindConstantByNumber(
      int64_t number) const override;

 private:
  class Iterator : public EnumType::ConstantIterator {
   public:
    using Constant = EnumType::Constant;

    explicit Iterator(absl::Span<const Enumerator> enumerators)
        : idx_(0), enumerators_(enumerators) {}

    bool HasNext() override { return idx_ < enumerators_.size(); }

    absl::StatusOr<Constant> Next() override {
      if (!HasNext()) {
        return absl::FailedPreconditionError(
            "Next() called when HasNext() false in "
            "ResolveableEnumType::Iterator");
      }
      int current = idx_;
      idx_++;
      return Constant(MakeConstantId(enumerators_[current].number),
                      enumerators_[current].name, enumerators_[current].number);
    }

    absl::StatusOr<absl::string_view> NextName() override {
      CEL_ASSIGN_OR_RETURN(Constant constant, Next());

      return constant.name;
    }

    absl::StatusOr<int64_t> NextNumber() override {
      CEL_ASSIGN_OR_RETURN(Constant constant, Next());

      return constant.number;
    }

   private:
    // The index for the next returned value.
    int idx_;
    absl::Span<const Enumerator> enumerators_;
  };

  // Implement EnumType.
  cel::internal::TypeInfo TypeId() const override {
    return cel::internal::TypeId<ResolveableEnumType>();
  }

  std::string name_;
  // TODO(issues/5): this could be indexed by name and/or number if strong
  // enum typing is needed at runtime.
  std::vector<Enumerator> enumerators_;
};

void AddEnumFromDescriptor(const google::protobuf::EnumDescriptor* desc,
                           CelTypeRegistry& registry) {
  std::vector<CelTypeRegistry::Enumerator> enumerators;
  enumerators.reserve(desc->value_count());
  for (int i = 0; i < desc->value_count(); i++) {
    enumerators.push_back({desc->value(i)->name(), desc->value(i)->number()});
  }
  registry.RegisterEnum(desc->full_name(), std::move(enumerators));
}

// Portable version. Add overloads for specific core supported enums.
template <typename T, typename U = void>
struct EnumAdderT {
  template <typename EnumT>
  void AddEnum(DescriptorSet&) {}
};

template <typename T>
struct EnumAdderT<T, typename std::enable_if<
                         std::is_base_of_v<google::protobuf::Message, T>, void>::type> {
  template <typename EnumT>
  void AddEnum(DescriptorSet& set) {
    set.insert(google::protobuf::GetEnumDescriptor<EnumT>());
  }
};

// Enable loading the linked descriptor if using the full proto runtime.
// Otherwise, only support explicitly defined enums.
using EnumAdder = EnumAdderT<google::protobuf::Struct>;

const absl::flat_hash_set<const google::protobuf::EnumDescriptor*>& GetCoreEnums() {
  static cel::internal::NoDestructor<DescriptorSet> kCoreEnums([]() {
    absl::flat_hash_set<const google::protobuf::EnumDescriptor*> instance;
    EnumAdder().AddEnum<google::protobuf::NullValue>(instance);
    return instance;
  }());
  return *kCoreEnums;
}

}  // namespace

absl::StatusOr<absl::optional<ResolveableEnumType::Constant>>
ResolveableEnumType::FindConstantByName(absl::string_view name) const {
  for (const Enumerator& enumerator : enumerators_) {
    if (enumerator.name == name) {
      return ResolveableEnumType::Constant(MakeConstantId(enumerator.number),
                                           enumerator.name, enumerator.number);
    }
  }
  return absl::nullopt;
}

absl::StatusOr<absl::optional<ResolveableEnumType::Constant>>
ResolveableEnumType::FindConstantByNumber(int64_t number) const {
  for (const Enumerator& enumerator : enumerators_) {
    if (enumerator.number == number) {
      return ResolveableEnumType::Constant(MakeConstantId(enumerator.number),
                                           enumerator.name, enumerator.number);
    }
  }
  return absl::nullopt;
}

CelTypeRegistry::CelTypeRegistry()
    : types_(GetCoreTypes()), enums_(GetCoreEnums()) {
  RegisterEnum("google.protobuf.NullValue", {{"NULL_VALUE", 0}});
}

void CelTypeRegistry::Register(std::string fully_qualified_type_name) {
  // Registers the fully qualified type name as a CEL type.
  absl::MutexLock lock(&mutex_);
  types_.insert(std::move(fully_qualified_type_name));
}

void CelTypeRegistry::Register(const google::protobuf::EnumDescriptor* enum_descriptor) {
  enums_.insert(enum_descriptor);
  AddEnumFromDescriptor(enum_descriptor, *this);
}

void CelTypeRegistry::RegisterEnum(absl::string_view enum_name,
                                   std::vector<Enumerator> enumerators) {
  absl::StatusOr<cel::Handle<cel::EnumType>> result_or =
      GetDefaultTypeFactory().CreateEnumType<ResolveableEnumType>(
          std::string(enum_name), std::move(enumerators));
  // For this setup, the type factory should never return an error.
  result_or.IgnoreError();
  resolveable_enums_[enum_name] = std::move(result_or).value();
}

std::shared_ptr<const LegacyTypeProvider>
CelTypeRegistry::GetFirstTypeProvider() const {
  if (type_providers_.empty()) {
    return nullptr;
  }
  return type_providers_[0];
}

// Find a type's CelValue instance by its fully qualified name.
absl::optional<LegacyTypeAdapter> CelTypeRegistry::FindTypeAdapter(
    absl::string_view fully_qualified_type_name) const {
  for (const auto& provider : type_providers_) {
    auto maybe_adapter = provider->ProvideLegacyType(fully_qualified_type_name);
    if (maybe_adapter.has_value()) {
      return maybe_adapter;
    }
  }

  return absl::nullopt;
}

cel::Handle<cel::Value> CelTypeRegistry::FindType(
    absl::string_view fully_qualified_type_name) const {
  // String canonical type names are interned in the node hash set.
  // Some types are lazily provided by the registered type providers, so
  // synchronization is needed to preserve const correctness.
  absl::MutexLock lock(&mutex_);
  // Searches through explicitly registered type names first.
  auto type = types_.find(fully_qualified_type_name);
  // The CelValue returned by this call will remain valid as long as the
  // CelExpression and associated builder stay in scope.
  if (type != types_.end()) {
    return CreateTypeValueFromView(*type);
  }

  // By default falls back to looking at whether the type is provided by one
  // of the registered providers (generally, one backed by the generated
  // DescriptorPool).
  auto adapter = FindTypeAdapter(fully_qualified_type_name);
  if (adapter.has_value()) {
    auto [iter, inserted] =
        types_.insert(std::string(fully_qualified_type_name));
    return CreateTypeValueFromView(*iter);
  }

  return cel::Handle<Value>();
}

}  // namespace google::api::expr::runtime
