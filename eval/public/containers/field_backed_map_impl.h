#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_FIELD_BACKED_MAP_IMPL_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_FIELD_BACKED_MAP_IMPL_H_

#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "absl/status/statusor.h"
#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// CelMap implementation that uses "map" message field
// as backing storage.
class FieldBackedMapImpl : public CelMap {
 public:
  // message contains the "map" field. Object stores the pointer
  // to the message, thus it is expected that message outlives the
  // object.
  // descriptor FieldDescriptor for the field
  FieldBackedMapImpl(const google::protobuf::Message* message,
                     const google::protobuf::FieldDescriptor* descriptor,
                     google::protobuf::Arena* arena);

  // Map size.
  int size() const override;

  // Map element access operator.
  absl::optional<CelValue> operator[](CelValue key) const override;

  // Presence test function.
  absl::StatusOr<bool> Has(const CelValue& key) const override;

  const CelList* ListKeys() const override;

 protected:
  // These methods are exposed as protected methods for testing purposes since
  // whether one or the other is used depends on build time flags, but each
  // should be tested accordingly.

  absl::StatusOr<bool> LookupMapValue(
      const CelValue& key, google::protobuf::MapValueConstRef* value_ref) const;

  absl::StatusOr<bool> LegacyHasMapValue(const CelValue& key) const;

  absl::optional<CelValue> LegacyLookupMapValue(const CelValue& key) const;

 private:
  const google::protobuf::Message* message_;
  const google::protobuf::FieldDescriptor* descriptor_;
  const google::protobuf::FieldDescriptor* key_desc_;
  const google::protobuf::FieldDescriptor* value_desc_;
  const google::protobuf::Reflection* reflection_;
  google::protobuf::Arena* arena_;
  std::unique_ptr<CelList> key_list_;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_FIELD_BACKED_MAP_IMPL_H_
