// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "eval/public/structs/legacy_type_provider.h"

#include <optional>
#include <string>

#include "eval/public/structs/legacy_any_packing.h"
#include "eval/public/structs/legacy_type_info_apis.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {
namespace {

class LegacyTypeProviderTestEmpty : public LegacyTypeProvider {
 public:
  absl::optional<LegacyTypeAdapter> ProvideLegacyType(
      absl::string_view name) const override {
    return absl::nullopt;
  }
};

class LegacyTypeInfoApisEmpty : public LegacyTypeInfoApis {
 public:
  std::string DebugString(
      const MessageWrapper& wrapped_message) const override {
    return "";
  }
  const std::string& GetTypename(
      const MessageWrapper& wrapped_message) const override {
    return test_string_;
  }
  const LegacyTypeAccessApis* GetAccessApis(
      const MessageWrapper& wrapped_message) const override {
    return nullptr;
  }

 private:
  const std::string test_string_ = "test";
};

class LegacyAnyPackingApisEmpty : public LegacyAnyPackingApis {
 public:
  absl::StatusOr<google::protobuf::MessageLite*> Unpack(
      const google::protobuf::Any& any_message,
      google::protobuf::Arena* arena) const override {
    return absl::UnimplementedError("Unimplemented Unpack");
  }
  absl::Status Pack(const google::protobuf::MessageLite* message,
                    google::protobuf::Any& any_message) const override {
    return absl::UnimplementedError("Unimplemented Pack");
  }
};

class LegacyTypeProviderTestImpl : public LegacyTypeProvider {
 public:
  explicit LegacyTypeProviderTestImpl(
      const LegacyTypeInfoApis* test_type_info,
      const LegacyAnyPackingApis* test_any_packing_apis)
      : test_type_info_(test_type_info),
        test_any_packing_apis_(test_any_packing_apis) {}
  absl::optional<LegacyTypeAdapter> ProvideLegacyType(
      absl::string_view name) const override {
    if (name == "test") {
      return LegacyTypeAdapter(nullptr, nullptr);
    }
    return absl::nullopt;
  }
  absl::optional<const LegacyTypeInfoApis*> ProvideLegacyTypeInfo(
      absl::string_view name) const override {
    if (name == "test") {
      return test_type_info_;
    }
    return absl::nullopt;
  }
  absl::optional<const LegacyAnyPackingApis*> ProvideLegacyAnyPackingApis(
      absl::string_view name) const override {
    if (name == "test") {
      return test_any_packing_apis_;
    }
    return absl::nullopt;
  }

 private:
  const LegacyTypeInfoApis* test_type_info_ = nullptr;
  const LegacyAnyPackingApis* test_any_packing_apis_ = nullptr;
};

TEST(LegacyTypeProviderTest, EmptyTypeProviderHasProvideTypeInfo) {
  LegacyTypeProviderTestEmpty provider;
  EXPECT_EQ(provider.ProvideLegacyType("test"), absl::nullopt);
  EXPECT_EQ(provider.ProvideLegacyTypeInfo("test"), absl::nullopt);
  EXPECT_EQ(provider.ProvideLegacyAnyPackingApis("test"), absl::nullopt);
}

TEST(LegacyTypeProviderTest, NonEmptyTypeProviderProvidesSomeTypes) {
  LegacyTypeInfoApisEmpty test_type_info;
  LegacyAnyPackingApisEmpty test_any_packing_apis;
  LegacyTypeProviderTestImpl provider(&test_type_info, &test_any_packing_apis);
  EXPECT_TRUE(provider.ProvideLegacyType("test").has_value());
  EXPECT_TRUE(provider.ProvideLegacyTypeInfo("test").has_value());
  EXPECT_TRUE(provider.ProvideLegacyAnyPackingApis("test").has_value());
  EXPECT_EQ(provider.ProvideLegacyType("other"), absl::nullopt);
  EXPECT_EQ(provider.ProvideLegacyTypeInfo("other"), absl::nullopt);
  EXPECT_EQ(provider.ProvideLegacyAnyPackingApis("other"), absl::nullopt);
}

}  // namespace
}  // namespace google::api::expr::runtime
