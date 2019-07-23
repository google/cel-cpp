#include "common/type.h"

#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"

namespace google {
namespace api {
namespace expr {
namespace common {

namespace {

constexpr const std::size_t kBasicTypeNamesSize = 10;
const auto* kBasicTypeNames = new std::array<std::string, kBasicTypeNamesSize>({
    "null_type",  // kNull
    "bool",       // kBool
    "int",        // kInt
    "uint",       // kUInt
    "double",     // kDouble
    "string",     // kString,
    "bytes",      // kBytes
    "type",       // kType
    "map",        // kMap
    "list",       // kList
});

static_assert(kBasicTypeNamesSize ==
                  static_cast<int>(BasicTypeValue::DO_NOT_USE),
              "unexpected size");

static const std::map<absl::string_view, BasicType>* const kBasicTypeMap =
    []() {
      auto result = new std::map<absl::string_view, BasicType>();
      for (std::size_t i = 0; i < kBasicTypeNames->size(); ++i) {
        result->emplace(kBasicTypeNames->at(i),
                        BasicType(static_cast<BasicTypeValue>(i)));
      }
      return result;
    }();

struct ToStringVisitor {
  template <typename T>
  const std::string& operator()(const T& value) {
    return value.ToString();
  }
};

struct FullNameVisitor {
  template <typename T>
  absl::string_view operator()(const T& value) {
    return value.full_name();
  }
};

}  // namespace

const std::string& BasicType::ToString() const {
  return kBasicTypeNames->at(static_cast<std::size_t>(value_));
}

std::unique_ptr<google::protobuf::Message> ObjectType::Unpack(
    const google::protobuf::Any& value) {
  auto msg = absl::WrapUnique(
      google::protobuf::MessageFactory::generated_factory()->GetPrototype(value_)->New());
  if (!value.UnpackTo(msg.get())) {
    return nullptr;
  }
  return msg;
}

UnrecognizedType::UnrecognizedType(absl::string_view full_name)
    : string_rep_(absl::StrCat("type(\"", full_name, "\")")),
      hash_code_(internal::Hash(full_name)) {
  assert(google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
             std::string(full_name)) == nullptr);
}

absl::string_view UnrecognizedType::full_name() const {
  return absl::string_view(string_rep_).substr(6, string_rep_.size() - 8);
}

Type::Type(const std::string& full_name) : data_(BasicType(BasicTypeValue::kNull)) {
  auto itr = kBasicTypeMap->find(full_name);
  if (itr != kBasicTypeMap->end()) {
    data_ = itr->second;
    return;
  }

  auto obj_desc =
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          full_name);
  if (obj_desc != nullptr) {
    data_ = ObjectType(obj_desc);
    return;
  }

  auto enum_desc =
      google::protobuf::DescriptorPool::generated_pool()->FindEnumTypeByName(full_name);
  if (enum_desc != nullptr) {
    data_ = EnumType(enum_desc);
    return;
  }

  auto value = UnrecognizedType(full_name);
  data_ = value;
}

absl::string_view Type::full_name() const {
  return absl::visit(FullNameVisitor(), data_);
}

const std::string& Type::ToString() const {
  return absl::visit(ToStringVisitor(), data_);
}

}  // namespace common
}  // namespace expr
}  // namespace api
}  // namespace google
