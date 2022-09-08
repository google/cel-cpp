#include "eval/public/cel_type_registry.h"

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
#include "eval/public/cel_value.h"
#include "internal/no_destructor.h"

namespace google::api::expr::runtime {

namespace {

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
using EnumMap =
    absl::flat_hash_map<std::string, std::vector<CelTypeRegistry::Enumerator>>;

void AddEnumFromDescriptor(const google::protobuf::EnumDescriptor* desc, EnumMap& map) {
  std::vector<CelTypeRegistry::Enumerator> enumerators;
  enumerators.reserve(desc->value_count());
  for (int i = 0; i < desc->value_count(); i++) {
    enumerators.push_back({desc->value(i)->name(), desc->value(i)->number()});
  }
  map.insert(std::pair(desc->full_name(), std::move(enumerators)));
}

// Portable version. Add overloads for specfic core supported enums.
template <typename T, typename U = void>
struct EnumAdderT {
  template <typename EnumT>
  void AddEnum(DescriptorSet&) {}

  template <typename EnumT>
  void AddEnum(EnumMap& map) {
    if constexpr (std::is_same_v<EnumT, google::protobuf::NullValue>) {
      map["google.protobuf.NullValue"] = {{"NULL_VALUE", 0}};
    }
  }
};

template <typename T>
struct EnumAdderT<T, typename std::enable_if<
                         std::is_base_of_v<google::protobuf::Message, T>, void>::type> {
  template <typename EnumT>
  void AddEnum(DescriptorSet& set) {
    set.insert(google::protobuf::GetEnumDescriptor<EnumT>());
  }

  template <typename EnumT>
  void AddEnum(EnumMap& map) {
    const google::protobuf::EnumDescriptor* desc = google::protobuf::GetEnumDescriptor<EnumT>();
    AddEnumFromDescriptor(desc, map);
  }
};

// Enable loading the linked descriptor if using the full proto runtime.
// Otherwise, only support explcitly defined enums.
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

CelTypeRegistry::CelTypeRegistry()
    : types_(GetCoreTypes()), enums_(GetCoreEnums()) {
  EnumAdder().AddEnum<google::protobuf::NullValue>(enums_map_);
}

void CelTypeRegistry::Register(std::string fully_qualified_type_name) {
  // Registers the fully qualified type name as a CEL type.
  absl::MutexLock lock(&mutex_);
  types_.insert(std::move(fully_qualified_type_name));
}

void CelTypeRegistry::Register(const google::protobuf::EnumDescriptor* enum_descriptor) {
  enums_.insert(enum_descriptor);
  AddEnumFromDescriptor(enum_descriptor, enums_map_);
}

void CelTypeRegistry::RegisterEnum(absl::string_view enum_name,
                                   std::vector<Enumerator> enumerators) {
  enums_map_[enum_name] = std::move(enumerators);
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

absl::optional<CelValue> CelTypeRegistry::FindType(
    absl::string_view fully_qualified_type_name) const {
  absl::MutexLock lock(&mutex_);
  // Searches through explicitly registered type names first.
  auto type = types_.find(fully_qualified_type_name);
  // The CelValue returned by this call will remain valid as long as the
  // CelExpression and associated builder stay in scope.
  if (type != types_.end()) {
    return CelValue::CreateCelTypeView(*type);
  }

  // By default falls back to looking at whether the type is provided by one
  // of the registered providers (generally, one backed by the generated
  // DescriptorPool).
  auto adapter = FindTypeAdapter(fully_qualified_type_name);
  if (adapter.has_value()) {
    auto [iter, inserted] =
        types_.insert(std::string(fully_qualified_type_name));
    return CelValue::CreateCelTypeView(*iter);
  }
  return absl::nullopt;
}

}  // namespace google::api::expr::runtime
