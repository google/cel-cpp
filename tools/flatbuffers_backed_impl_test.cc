#include "tools/flatbuffers_backed_impl.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "flatbuffers/idl.h"
#include "flatbuffers/reflection.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using google::protobuf::Arena;

constexpr char kReflectionBufferPath[] =
    "tools/flatbuffers.bfbs";

constexpr absl::string_view kByteField = "f_byte";
constexpr absl::string_view kUbyteField = "f_ubyte";
constexpr absl::string_view kShortField = "f_short";
constexpr absl::string_view kUshortField = "f_ushort";
constexpr absl::string_view kIntField = "f_int";
constexpr absl::string_view kUintField = "f_uint";
constexpr absl::string_view kLongField = "f_long";
constexpr absl::string_view kUlongField = "f_ulong";
constexpr absl::string_view kFloatField = "f_float";
constexpr absl::string_view kDoubleField = "f_double";
constexpr absl::string_view kBoolField = "f_bool";
constexpr absl::string_view kStringField = "f_string";
constexpr absl::string_view kObjField = "f_obj";

constexpr absl::string_view kUnknownField = "f_unknown";

constexpr absl::string_view kBytesField = "r_byte";
constexpr absl::string_view kUbytesField = "r_ubyte";
constexpr absl::string_view kShortsField = "r_short";
constexpr absl::string_view kUshortsField = "r_ushort";
constexpr absl::string_view kIntsField = "r_int";
constexpr absl::string_view kUintsField = "r_uint";
constexpr absl::string_view kLongsField = "r_long";
constexpr absl::string_view kUlongsField = "r_ulong";
constexpr absl::string_view kFloatsField = "r_float";
constexpr absl::string_view kDoublesField = "r_double";
constexpr absl::string_view kBoolsField = "r_bool";
constexpr absl::string_view kStringsField = "r_string";
constexpr absl::string_view kObjsField = "r_obj";
constexpr absl::string_view kIndexedField = "r_indexed";

const int64_t kNumFields = 27;

class FlatBuffersTest : public testing::Test {
 public:
  FlatBuffersTest() {
    EXPECT_TRUE(
        flatbuffers::LoadFile(kReflectionBufferPath, true, &schema_file_));
    flatbuffers::Verifier verifier(
        reinterpret_cast<const uint8_t*>(schema_file_.data()),
        schema_file_.size());
    EXPECT_TRUE(reflection::VerifySchemaBuffer(verifier));
    EXPECT_TRUE(parser_.Deserialize(
        reinterpret_cast<const uint8_t*>(schema_file_.data()),
        schema_file_.size()));
    schema_ = reflection::GetSchema(schema_file_.data());
  }
  const CelMap& loadJson(std::string data) {
    EXPECT_TRUE(parser_.Parse(data.data()));
    const CelMap* value = CreateFlatBuffersBackedObject(
        parser_.builder_.GetBufferPointer(), *schema_, &arena_);
    EXPECT_NE(nullptr, value);
    EXPECT_EQ(kNumFields, value->size());
    const CelList* keys = value->ListKeys();
    EXPECT_NE(nullptr, keys);
    EXPECT_EQ(kNumFields, keys->size());
    EXPECT_TRUE((*keys)[2].IsString());
    return *value;
  }

 protected:
  std::string schema_file_;
  flatbuffers::Parser parser_;
  const reflection::Schema* schema_;
  google::protobuf::Arena arena_;
};

TEST_F(FlatBuffersTest, PrimitiveFields) {
  const CelMap& value = loadJson(R"({
              f_byte: -1,
              f_ubyte: 1,
              f_short: -2,
              f_ushort: 2,
              f_int: -3,
              f_uint: 3,
              f_long: -4,
              f_ulong: 4,
              f_float: 5.0,
              f_double: 6.0,
              f_bool: false,
              f_string: "test"
              })");
  // byte
  {
    auto f = value[CelValue::CreateStringView(kByteField)];
    EXPECT_TRUE(f.has_value());
    EXPECT_TRUE(f.value().IsInt64());
    EXPECT_EQ(-1, f.value().Int64OrDie());
  }
  {
    auto uf = value[CelValue::CreateStringView(kUbyteField)];
    EXPECT_TRUE(uf.has_value());
    EXPECT_TRUE(uf.value().IsUint64());
    EXPECT_EQ(1, uf.value().Uint64OrDie());
  }
  // short
  {
    auto f = value[CelValue::CreateStringView(kShortField)];
    EXPECT_TRUE(f.has_value());
    EXPECT_TRUE(f.value().IsInt64());
    EXPECT_EQ(-2, f.value().Int64OrDie());
  }
  {
    auto uf = value[CelValue::CreateStringView(kUshortField)];
    EXPECT_TRUE(uf.has_value());
    EXPECT_TRUE(uf.value().IsUint64());
    EXPECT_EQ(2, uf.value().Uint64OrDie());
  }
  // int
  {
    auto f = value[CelValue::CreateStringView(kIntField)];
    EXPECT_TRUE(f.has_value());
    EXPECT_TRUE(f.value().IsInt64());
    EXPECT_EQ(-3, f.value().Int64OrDie());
  }
  {
    auto uf = value[CelValue::CreateStringView(kUintField)];
    EXPECT_TRUE(uf.has_value());
    EXPECT_TRUE(uf.value().IsUint64());
    EXPECT_EQ(3, uf.value().Uint64OrDie());
  }
  // long
  {
    auto f = value[CelValue::CreateStringView(kLongField)];
    EXPECT_TRUE(f.has_value());
    EXPECT_TRUE(f.value().IsInt64());
    EXPECT_EQ(-4, f.value().Int64OrDie());
  }
  {
    auto uf = value[CelValue::CreateStringView(kUlongField)];
    EXPECT_TRUE(uf.has_value());
    EXPECT_TRUE(uf.value().IsUint64());
    EXPECT_EQ(4, uf.value().Uint64OrDie());
  }
  // float and double
  {
    auto f = value[CelValue::CreateStringView(kFloatField)];
    EXPECT_TRUE(f.has_value());
    EXPECT_TRUE(f.value().IsDouble());
    EXPECT_EQ(5.0, f.value().DoubleOrDie());
  }
  {
    auto f = value[CelValue::CreateStringView(kDoubleField)];
    EXPECT_TRUE(f.has_value());
    EXPECT_TRUE(f.value().IsDouble());
    EXPECT_EQ(6.0, f.value().DoubleOrDie());
  }
  // bool
  {
    auto f = value[CelValue::CreateStringView(kBoolField)];
    EXPECT_TRUE(f.has_value());
    EXPECT_TRUE(f.value().IsBool());
    EXPECT_EQ(false, f.value().BoolOrDie());
  }
  // string
  {
    auto f = value[CelValue::CreateStringView(kStringField)];
    EXPECT_TRUE(f.has_value());
    EXPECT_TRUE(f.value().IsString());
    EXPECT_EQ("test", f.value().StringOrDie().value());
  }
  // missing field
  {
    auto f = value[CelValue::CreateInt64(1)];
    EXPECT_FALSE(f.has_value());
  }
  {
    auto f = value[CelValue::CreateStringView(kUnknownField)];
    EXPECT_FALSE(f.has_value());
  }
}

TEST_F(FlatBuffersTest, PrimitiveFieldDefaults) {
  const CelMap& value = loadJson("{}");
  // byte
  {
    auto f = value[CelValue::CreateStringView(kByteField)];
    EXPECT_TRUE(f.has_value());
    EXPECT_TRUE(f.value().IsInt64());
    EXPECT_EQ(0, f.value().Int64OrDie());
  }
  // short
  {
    auto f = value[CelValue::CreateStringView(kShortField)];
    EXPECT_TRUE(f.has_value());
    EXPECT_TRUE(f.value().IsInt64());
    EXPECT_EQ(150, f.value().Int64OrDie());
  }
  // bool
  {
    auto f = value[CelValue::CreateStringView(kBoolField)];
    EXPECT_TRUE(f.has_value());
    EXPECT_TRUE(f.value().IsBool());
    EXPECT_EQ(true, f.value().BoolOrDie());
  }
  // string
  {
    auto f = value[CelValue::CreateStringView(kStringField)];
    EXPECT_TRUE(f.has_value());
    EXPECT_TRUE(f.value().IsString());
    EXPECT_EQ("", f.value().StringOrDie().value());
  }
}

TEST_F(FlatBuffersTest, ObjectField) {
  const CelMap& value = loadJson(R"({
                                    f_obj: {
                                      f_string: "entry",
                                      f_int: 16
                                    }
                                    })");
  auto f = value[CelValue::CreateStringView(kObjField)];
  EXPECT_TRUE(f.has_value());
  EXPECT_TRUE(f.value().IsMap());
  const CelMap& m = *f.value().MapOrDie();
  EXPECT_EQ(2, m.size());
  {
    auto mf = m[CelValue::CreateStringView(kStringField)];
    EXPECT_TRUE(mf.has_value());
    EXPECT_TRUE(mf.value().IsString());
    EXPECT_EQ("entry", mf.value().StringOrDie().value());
  }
  {
    auto mf = m[CelValue::CreateStringView(kIntField)];
    EXPECT_TRUE(mf.has_value());
    EXPECT_TRUE(mf.value().IsInt64());
    EXPECT_EQ(16, mf.value().Int64OrDie());
  }
}

TEST_F(FlatBuffersTest, ObjectFieldDefault) {
  const CelMap& value = loadJson("{}");
  auto f = value[CelValue::CreateStringView(kObjField)];
  EXPECT_TRUE(f.has_value());
  EXPECT_TRUE(f.value().IsNull());
}

TEST_F(FlatBuffersTest, PrimitiveVectorFields) {
  const CelMap& value = loadJson(R"({
              r_byte: [-97],
              r_ubyte: [97, 98, 99],
              r_short: [-2],
              r_ushort: [2],
              r_int: [-3],
              r_uint: [3],
              r_long: [-4],
              r_ulong: [4],
              r_float: [5.0],
              r_double: [6.0],
              r_bool: [false],
              r_string: ["test"]
              })");
  // byte
  {
    auto f = value[CelValue::CreateStringView(kBytesField)];
    EXPECT_TRUE(f.has_value());
    EXPECT_TRUE(f.value().IsBytes());
    EXPECT_EQ("\x9F", f.value().BytesOrDie().value());
  }
  {
    auto uf = value[CelValue::CreateStringView(kUbytesField)];
    EXPECT_TRUE(uf.has_value());
    EXPECT_TRUE(uf.value().IsBytes());
    EXPECT_EQ("abc", uf.value().BytesOrDie().value());
  }
  // short
  {
    auto f = value[CelValue::CreateStringView(kShortsField)];
    EXPECT_TRUE(f.has_value());
    EXPECT_TRUE(f.value().IsList());
    const CelList& l = *f.value().ListOrDie();
    EXPECT_EQ(1, l.size());
    EXPECT_EQ(-2, l[0].Int64OrDie());
  }
  {
    auto uf = value[CelValue::CreateStringView(kUshortsField)];
    EXPECT_TRUE(uf.has_value());
    EXPECT_TRUE(uf.value().IsList());
    const CelList& l = *uf.value().ListOrDie();
    EXPECT_EQ(1, l.size());
    EXPECT_EQ(2, l[0].Uint64OrDie());
  }
  // int
  {
    auto f = value[CelValue::CreateStringView(kIntsField)];
    EXPECT_TRUE(f.has_value());
    EXPECT_TRUE(f.value().IsList());
    const CelList& l = *f.value().ListOrDie();
    EXPECT_EQ(1, l.size());
    EXPECT_EQ(-3, l[0].Int64OrDie());
  }
  {
    auto uf = value[CelValue::CreateStringView(kUintsField)];
    EXPECT_TRUE(uf.has_value());
    EXPECT_TRUE(uf.value().IsList());
    const CelList& l = *uf.value().ListOrDie();
    EXPECT_EQ(1, l.size());
    EXPECT_EQ(3, l[0].Uint64OrDie());
  }
  // long
  {
    auto f = value[CelValue::CreateStringView(kLongsField)];
    EXPECT_TRUE(f.has_value());
    EXPECT_TRUE(f.value().IsList());
    const CelList& l = *f.value().ListOrDie();
    EXPECT_EQ(1, l.size());
    EXPECT_EQ(-4, l[0].Int64OrDie());
  }
  {
    auto uf = value[CelValue::CreateStringView(kUlongsField)];
    EXPECT_TRUE(uf.has_value());
    EXPECT_TRUE(uf.value().IsList());
    const CelList& l = *uf.value().ListOrDie();
    EXPECT_EQ(1, l.size());
    EXPECT_EQ(4, l[0].Uint64OrDie());
  }
  // float and double
  {
    auto f = value[CelValue::CreateStringView(kFloatsField)];
    EXPECT_TRUE(f.has_value());
    EXPECT_TRUE(f.value().IsList());
    const CelList& l = *f.value().ListOrDie();
    EXPECT_EQ(1, l.size());
    EXPECT_EQ(5.0, l[0].DoubleOrDie());
  }
  {
    auto f = value[CelValue::CreateStringView(kDoublesField)];
    EXPECT_TRUE(f.has_value());
    EXPECT_TRUE(f.value().IsList());
    const CelList& l = *f.value().ListOrDie();
    EXPECT_EQ(1, l.size());
    EXPECT_EQ(6.0, l[0].DoubleOrDie());
  }
  // bool
  {
    auto f = value[CelValue::CreateStringView(kBoolsField)];
    EXPECT_TRUE(f.has_value());
    EXPECT_TRUE(f.value().IsList());
    const CelList& l = *f.value().ListOrDie();
    EXPECT_EQ(1, l.size());
    EXPECT_EQ(false, l[0].BoolOrDie());
  }
  // string
  {
    auto f = value[CelValue::CreateStringView(kStringsField)];
    EXPECT_TRUE(f.has_value());
    EXPECT_TRUE(f.value().IsList());
    const CelList& l = *f.value().ListOrDie();
    EXPECT_EQ(1, l.size());
    EXPECT_EQ("test", l[0].StringOrDie().value());
  }
}

TEST_F(FlatBuffersTest, ObjectVectorField) {
  const CelMap& value = loadJson(R"({
                                    r_obj: [{
                                      f_string: "entry",
                                      f_int: 16
                                    },{
                                      f_int: 32
                                    }]
                                    })");
  auto f = value[CelValue::CreateStringView(kObjsField)];
  EXPECT_TRUE(f.has_value());
  EXPECT_TRUE(f.value().IsList());
  const CelList& l = *f.value().ListOrDie();
  EXPECT_EQ(2, l.size());
  {
    EXPECT_TRUE(l[0].IsMap());
    const CelMap& m = *l[0].MapOrDie();
    EXPECT_EQ(2, m.size());
    {
      auto mf = m[CelValue::CreateStringView(kStringField)];
      EXPECT_TRUE(mf.has_value());
      EXPECT_TRUE(mf.value().IsString());
      EXPECT_EQ("entry", mf.value().StringOrDie().value());
    }
    {
      auto mf = m[CelValue::CreateStringView(kIntField)];
      EXPECT_TRUE(mf.has_value());
      EXPECT_TRUE(mf.value().IsInt64());
      EXPECT_EQ(16, mf.value().Int64OrDie());
    }
  }
  {
    EXPECT_TRUE(l[1].IsMap());
    const CelMap& m = *l[1].MapOrDie();
    EXPECT_EQ(2, m.size());
    {
      auto mf = m[CelValue::CreateStringView(kStringField)];
      EXPECT_TRUE(mf.has_value());
      EXPECT_TRUE(mf.value().IsString());
      EXPECT_EQ("", mf.value().StringOrDie().value());
    }
    {
      auto mf = m[CelValue::CreateStringView(kIntField)];
      EXPECT_TRUE(mf.has_value());
      EXPECT_TRUE(mf.value().IsInt64());
      EXPECT_EQ(32, mf.value().Int64OrDie());
    }
  }
}

TEST_F(FlatBuffersTest, VectorFieldDefaults) {
  const CelMap& value = loadJson("{}");
  for (const auto field : std::vector<absl::string_view>{
           kIntsField, kBoolsField, kStringsField, kObjsField}) {
    auto f = value[CelValue::CreateStringView(field)];
    EXPECT_TRUE(f.has_value());
    EXPECT_TRUE(f.value().IsList());
    const CelList& l = *f.value().ListOrDie();
    EXPECT_EQ(0, l.size());
  }

  {
    auto f = value[CelValue::CreateStringView(kIndexedField)];
    EXPECT_TRUE(f.has_value());
    EXPECT_TRUE(f.value().IsMap());
    const CelMap& m = *f.value().MapOrDie();
    EXPECT_EQ(0, m.size());
    EXPECT_EQ(0, m.ListKeys()->size());
  }

  {
    auto f = value[CelValue::CreateStringView(kBytesField)];
    EXPECT_TRUE(f.has_value());
    EXPECT_TRUE(f.value().IsBytes());
    EXPECT_EQ("", f.value().BytesOrDie().value());
  }
}

TEST_F(FlatBuffersTest, IndexedObjectVectorField) {
  const CelMap& value = loadJson(R"({
                                    r_indexed: [
                                    {
                                      f_string: "a",
                                      f_int: 16
                                    },
                                    {
                                      f_string: "b",
                                      f_int: 32
                                    },
                                    {
                                      f_string: "c",
                                      f_int: 64
                                    },
                                    {
                                      f_string: "d",
                                      f_int: 128
                                    }
                                    ]
                                    })");
  auto f = value[CelValue::CreateStringView(kIndexedField)];
  EXPECT_TRUE(f.has_value());
  EXPECT_TRUE(f.value().IsMap());
  const CelMap& m = *f.value().MapOrDie();
  EXPECT_EQ(4, m.size());
  const CelList& l = *m.ListKeys();
  EXPECT_EQ(4, l.size());
  EXPECT_TRUE(l[0].IsString());
  EXPECT_TRUE(l[1].IsString());
  EXPECT_TRUE(l[2].IsString());
  EXPECT_TRUE(l[3].IsString());
  std::string a = "a";
  std::string b = "b";
  std::string c = "c";
  std::string d = "d";
  EXPECT_EQ(a, l[0].StringOrDie().value());
  EXPECT_EQ(b, l[1].StringOrDie().value());
  EXPECT_EQ(c, l[2].StringOrDie().value());
  EXPECT_EQ(d, l[3].StringOrDie().value());

  for (const std::string& key : std::vector<std::string>{a, b, c, d}) {
    auto v = m[CelValue::CreateString(&key)];
    EXPECT_TRUE(v.has_value());
    const CelMap& vm = *v.value().MapOrDie();
    EXPECT_EQ(2, vm.size());
    auto vf = vm[CelValue::CreateStringView(kStringField)];
    EXPECT_TRUE(vf.has_value());
    EXPECT_TRUE(vf.value().IsString());
    EXPECT_EQ(key, vf.value().StringOrDie().value());
    auto vi = vm[CelValue::CreateStringView(kIntField)];
    EXPECT_TRUE(vi.has_value());
    EXPECT_TRUE(vi.value().IsInt64());
  }

  {
    std::string bb = "bb";
    std::string dd = "dd";
    EXPECT_FALSE(m[CelValue::CreateString(&bb)].has_value());
    EXPECT_FALSE(m[CelValue::CreateString(&dd)].has_value());
    EXPECT_FALSE(
        m[CelValue::CreateStringView(absl::string_view())].has_value());
  }
}

TEST_F(FlatBuffersTest, IndexedObjectVectorFieldDefaults) {
  const CelMap& value = loadJson(R"({
                                    r_indexed: [
                                    {
                                      f_string: "",
                                      f_int: 16
                                    }
                                    ]
                                    })");
  auto f = value[CelValue::CreateStringView(kIndexedField)];
  EXPECT_TRUE(f.has_value());
  EXPECT_TRUE(f.value().IsMap());
  const CelMap& m = *f.value().MapOrDie();
  EXPECT_EQ(1, m.size());
  const CelList& l = *m.ListKeys();
  EXPECT_EQ(1, l.size());
  EXPECT_TRUE(l[0].IsString());
  EXPECT_EQ("", l[0].StringOrDie().value());
  auto v = m[CelValue::CreateStringView(absl::string_view())];
  EXPECT_TRUE(v.has_value());
}

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
