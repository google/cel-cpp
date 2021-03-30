#include "eval/public/cel_type_registry.h"

#include "google/protobuf/struct.pb.h"
#include "google/protobuf/descriptor.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_set.h"
#include "absl/status/status.h"
#include "absl/types/optional.h"
#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

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

const absl::flat_hash_set<const google::protobuf::EnumDescriptor*> GetCoreEnums() {
  static const auto* const kCoreEnums =
      new absl::flat_hash_set<const google::protobuf::EnumDescriptor*>{
          // Register the NULL_VALUE enum.
          google::protobuf::NullValue_descriptor(),
      };
  return *kCoreEnums;
}

}  // namespace

CelTypeRegistry::CelTypeRegistry()
    : types_(GetCoreTypes()), enums_(GetCoreEnums()) {}

void CelTypeRegistry::Register(std::string fully_qualified_type_name) {
  // Registers the fully qualified type name as a CEL type.
  types_.insert(std::move(fully_qualified_type_name));
}

void CelTypeRegistry::Register(const google::protobuf::EnumDescriptor* enum_descriptor) {
  enums_.insert(enum_descriptor);
}

const google::protobuf::Descriptor* CelTypeRegistry::FindDescriptor(
    absl::string_view fully_qualified_type_name) const {
  return google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
      fully_qualified_type_name.data());
}

absl::optional<CelValue> CelTypeRegistry::FindType(
    absl::string_view fully_qualified_type_name) const {
  // Searches through explicitly registered type names first.
  auto type = types_.find(fully_qualified_type_name);
  // The CelValue returned by this call will remain valid as long as the
  // CelExpression and associated builder stay in scope.
  if (type != types_.end()) {
    return CelValue::CreateCelTypeView(*type);
  }

  // By default falls back to looking at whether the protobuf descriptor is
  // linked into the binary. In the future, this functionality may be disabled,
  // but this is most consistent with the current CEL C++ behavior.
  auto desc = FindDescriptor(fully_qualified_type_name);
  if (desc != nullptr) {
    return CelValue::CreateCelTypeView(desc->full_name());
  }
  return absl::nullopt;
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
