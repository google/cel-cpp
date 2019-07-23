#ifndef THIRD_PARTY_CEL_CPP_COMMON_CEL_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_CEL_VALUE_H_

#include <stdint.h>
#include <sys/types.h>
#include <initializer_list>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

#include "google/protobuf/any.pb.h"
#include "google/rpc/status.pb.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "common/enum.h"
#include "common/error.h"
#include "common/id.h"
#include "common/parent_ref.h"
#include "common/type.h"
#include "common/unknown.h"
#include "internal/hash_util.h"
#include "internal/holder.h"
#include "internal/ref_countable.h"
#include "internal/status_util.h"
#include "internal/value_internal.h"
#include "internal/visitor_util.h"

namespace google {
namespace api {
namespace expr {
namespace common {

/**
 * A CEL Value.
 *
 * Instances of this class can always be cheaply copied.
 *
 * The value held by a Value is either inlined, owned, or unowned. If
 * inlined, the value is embedded directly in the Value object. If owned, the
 * value is held via a shared pointer. If unowned, the value is held via a raw
 * pointer and the creator of the Value is responsible for insuring the
 * referenced value lives longer than the Value or any of its copies.
 *
 * Value provides a unified interface for all three cases.
 * However, `Value::is_inline()` and `Value::owns_value()` can be
 * used to distinguish these cases.
 *
 * Three types of constructor functions are provided:
 *  - From*: The resulting Value always owns the value being held. Depending
 * on the type, the value may be inlined or owned.
 *  - For*: The resulting Value might not own its value, instead only holding a
 *  pointer to its value. If so, the provided pointer argument must live longer
 *  than the resulting Value (or any copy of the resulting Value).
 *  - Make*: Helper functions that behave like std::make_unique. The resulting
 *  Value always owns its value.
 *
 * List, Map and Object base are not implemented directly, but rather base
 * classes for custom implementations. This allows for optimizations specific
 * to various contexts or data. For example, a homogenious map might use a
 * native c++ map to hold its data.
 */
class Value final : public internal::BaseValue {
 public:
  enum class Kind {
    // Values
    kNull,
    kBool,
    kInt,
    kUInt,
    kDouble,
    kString,
    kBytes,
    kType,
    kMap,
    kList,
    kObject,
    kEnum,

    // Objects that have well-known c++ representations.
    kDuration,
    kTime,

    // Non-values.
    kError,
    kUnknown,

    // Special value to require 'default' case in switch statements.
    DO_NOT_USE
  };

  // Constructors from primitive types.
  static inline Value NullValue() { return Create<kNull>(); }
  static inline Value FromBool(bool value) { return Create<kBool>(value); }
  static inline Value TrueValue() { return FromBool(true); }
  static inline Value FalseValue() { return FromBool(false); }
  static inline Value FromInt(int64_t value) { return Create<kInt>(value); }
  static inline Value FromUInt(uint64_t value) { return Create<kUInt>(value); }
  static inline Value FromDouble(double value);

  // Constructors from well-known c++ types.
  static inline Value FromString(absl::string_view value);
  static inline Value FromString(const std::string& value);
  static inline Value FromString(std::string&& value);
  // For string literals (e.g. const char*) use ForString.

  static inline Value FromBytes(absl::string_view value);
  static inline Value FromBytes(const std::string& value);
  static inline Value FromBytes(std::string&& value);
  // For byte literals (e.g. const char*) use ForBytes.

  static inline Value FromDuration(absl::Duration value);
  static inline Value FromTime(absl::Time value);

  static Value FromEnum(const EnumValue& value);
  static inline Value FromEnum(NamedEnumValue value);
  static inline Value FromEnum(const UnnamedEnumValue& value);

  static Value FromType(const Type& value);
  static inline Value FromType(BasicType value);
  static inline Value FromType(ObjectType value);
  static inline Value FromType(EnumType value);
  static inline Value FromType(BasicTypeValue value);
  static inline Value FromType(const char* value);
  static inline Value FromType(absl::string_view full_name);
  static inline Value FromType(const std::string& full_name);

  static inline Value FromError(const google::rpc::Status& value);
  static inline Value FromError(google::rpc::Status&& value);
  static inline Value FromError(const Error& value);
  static inline Value FromError(Error&& value);

  static Value FromUnknown(const Unknown& value);
  static Value FromUnknown(Unknown&& value);
  static inline Value FromUnknown(Id value) { return Create<kId>(value); }

  // Constructors *for* well-known c++ types.
  //
  // If no parent is passed in, any value referenced by the provided arguments
  // must live longer than the resulting Value (or any copy of the resulting
  // value).
  static Value ForString(absl::string_view value,
                         const ParentRef& parent = NoParent());
  static Value ForBytes(absl::string_view value,
                        const ParentRef& parent = NoParent());

  // Constructors from container types.
  static inline Value FromMap(std::unique_ptr<Map> value);
  static inline Value FromList(std::unique_ptr<List> value);
  static inline Value FromObject(std::unique_ptr<Object> value);

  // Constructors *for* Container types.
  //
  // The arguments passed in must live longer than any Value referencing
  // them.
  static inline Value ForMap(const Map* value);
  static inline Value ForList(const List* value);
  static inline Value ForObject(const Object* value);

  // Inplace constructors
  template <typename T, typename... Args>
  static Value MakeList(Args&&... args);
  template <typename T, typename... Args>
  static Value MakeMap(Args&&... args);
  template <typename T, typename... Args>
  static Value MakeObject(Args&&... args);

 private:
  template <Kind K>
  struct KindHelper;

  template <Kind K>
  using GetIfKType =
      GetIfType<internal::type_at<static_cast<std::size_t>(K), KindToType>>;

  template <Kind K>
  using GetKType =
      GetType<internal::type_at<static_cast<std::size_t>(K), KindToType>>;

 public:
  template <Kind K, typename T>
  static Value From(T&& value) {
    return KindHelper<K>::From(std::forward<T>(value));
  }

  template <Kind K, typename T>
  static Value For(T&& value, const ParentRef& parent = NoParent()) {
    return KindHelper<K>::For(std::forward<T>(value), parent);
  }

  Value() = default;
  ~Value();

  // Copy and move constructable.
  Value(const Value&) = default;
  Value(Value&) = default;
  Value(Value&&) = default;
  Value& operator=(const Value&) = default;
  Value& operator=(Value&&) = default;

  // Accessors.
  // Dies with absl::bad_variant_access if the wrong kind is accessed.

  // Mutable accessors for inlined values.
  inline bool& bool_value() { return *absl::get<kBool>(data_); }
  inline int64_t& int_value() { return *absl::get<kInt>(data_); }
  inline uint64_t& uint_value() { return *absl::get<kUInt>(data_); }
  inline double& double_value() { return *absl::get<kDouble>(data_); }

  // Const accessors for all value.
  inline bool is_null() const { return data_.index() == kNull; }
  inline const bool& bool_value() const;
  inline const int64_t& int_value() const;
  inline const uint64_t& uint_value() const;
  inline const double& double_value() const;
  inline const absl::Duration& duration() const;
  inline const absl::Time& time() const;
  inline const Error& error_value() const;
  inline const Map& map_value() const;
  inline const List& list_value() const;
  inline const Object& object_value() const;
  inline absl::string_view string_value() const;
  inline absl::string_view bytes_value() const;
  inline Type type_value() const;
  inline EnumValue enum_value() const;
  inline Unknown unknown_value() const;

  // Value metadata.
  /** If the value is stored directly in the Value */
  inline bool is_inline() const { return data_.index() < kInlineEnd; }

  /** If the value is a real 'value', and not an Error or Unknown. */
  inline bool is_value() const;

  /** If the value is considered an 'object'. */
  inline bool is_object() const { return index_in(kObject, kObjectEnd); }

  /** If the value is owned, or if an external value is being referenced. */
  bool owns_value() const;

  /** The kind of value stored. */
  Kind kind() const;

  /**
   * The hash code of this value.
   *
   * Cached internally when appropriate.
   */
  std::size_t hash_code() const;

  /**
   * Pure representation equality, e.g. Value(NaN) == Value(NaN) => true.
   */
  bool operator==(const Value& rhs) const;
  inline bool operator!=(const Value& rhs) const { return !(*this == rhs); }

  /** Applies the visitor to the given Values and returns the result. */
  template <typename V, typename F, typename... R>
  static VisitType<V, F, R...> visit(V&& vis, F&& value, R&&... rest);

  /** Applies the visitor and returns the result. */
  template <typename V>
  inline VisitType<V, Value> visit(V&& vis) const;

  template <typename T>
  inline GetType<T> get() const;
  template <Kind K>
  inline GetKType<K> get() const;

  template <typename T>
  inline GetIfType<T> get_if() const;
  template <Kind K>
  inline GetIfKType<K> get_if() const;

  /** Returns the type of the stored value. */
  Value GetType() const;

  /**
   * Returns a canonical cel expression for the value.
   *
   * Computation may be expensive.
   */
  std::string ToString() const;

 private:
  template <std::size_t I, typename... Args>
  static Value Create(Args&&... args);

  template <typename... Args>
  explicit Value(Args&&... args) : data_(std::forward<Args>(args)...) {}

  inline bool index_in(std::size_t start, std::size_t end) const;

  ValueData data_;
};

/** A base class for shared values that may contain other values. */
class Container : public SharedValue {
 public:
  /** The hash code for this value. Cached after the first call. */
  std::size_t hash_code() const;

 protected:
  Container();

  /**
   * The hash computation function.
   *
   * The result of this function is automatically cached.
   */
  virtual std::size_t ComputeHash() const = 0;

  // A helper alias that resolves to R iff the lambda type T returns a value
  // of type R when called. This is (annoyingly) needed as c++ does not match
  // overloads based on lambda return types by default.
  template <typename R, typename C>
  using ForEachReturnType = internal::specialize_if_returns<R, C>;

  // A helper function to convert a get result to a contains result.
  static Value GetToContainsResult(const Value& get_result);

  // Helpers to Convert a contained value into a cel Value.
  template <Value::Kind ValueKind, typename V>
  Value GetValue(V& value) {
    return Value::For<ValueKind>(&value, SelfRefProvider());
  }
  template <Value::Kind ValueKind, typename V>
  static Value GetValue(V&& value) {
    return Value::From<ValueKind>(std::move(value));
  }

 private:
  mutable std::atomic<std::size_t> hash_code_;
};

/** The base class for a CEL list value. */
class List : public Container {
 public:
  /** The number of elements in the list */
  virtual std::size_t size() const = 0;

  /** Returns the value at the index stored in value, or an error. */
  Value Get(const Value& value) const;

  /** Returns if the list contains the given value. */
  Value Contains(const Value& value) const;

  /** Returns the value at the given index, or an error. */
  virtual Value Get(std::size_t index) const = 0;

  /**
   * Calls the provided function with every element in the list, in order.
   *
   * If the provided function returns an error, iteration is stopped and that
   * error is returned to the caller immediately.
   */
  virtual google::rpc::Status ForEach(
      const std::function<google::rpc::Status(const Value&)>& call) const;

  /**
   * ForEach helper method for a boolean return type.
   *
   * Iteration stops when false is returned by `call`.
   */
  template <typename T>
  ForEachReturnType<bool, T(const Value&)> ForEach(T&& call) const;

  /**
   * ForEach helper method for a void return type.
   */
  template <typename T>
  ForEachReturnType<void, T(const Value&)> ForEach(T&& call) const;

  bool operator==(const List& rhs) const;
  inline bool operator!=(const List& rhs) { return !(*this == rhs); }

  std::string ToString() const override;

 protected:
  std::size_t ComputeHash() const override;
  virtual Value ContainsImpl(const Value& value) const;
};

/** The base class for a CEL map value. */
class Map : public Container {
 public:
  /** The number of elements in the map. */
  virtual std::size_t size() const = 0;

  /** Returns the value for the given key, or an error. */
  Value Get(const Value& key) const;

  /** Returns if the map contains the given key. */
  Value ContainsKey(const Value& key) const;

  /** Returns if the map contains the given value. */
  Value ContainsValue(const Value& value) const;

  /**
   * Calls the provided function with every entry in the map.
   *
   * If the provided function returns an error, iteration is stopped and that
   * error is returned to the caller immediately.
   */
  virtual google::rpc::Status ForEach(
      const std::function<google::rpc::Status(const Value&, const Value&)>&
          call) const = 0;
  /**
   * ForEach specialization for a boolean return type.
   *
   * Iteration stops if false is returned by `call`.
   */
  template <typename T>
  ForEachReturnType<bool, T(const Value&, const Value&)> ForEach(
      T&& call) const;

  /**
   * ForEach specialization for a void return type.
   */
  template <typename T>
  ForEachReturnType<void, T(const Value&, const Value&)> ForEach(
      T&& call) const;

  bool operator==(const Map& rhs) const;
  inline bool operator!=(const Map& rhs) const { return !(*this == rhs); }

  /**
   * Returns a cel expression for the value.
   *
   * Computation may be expensive.
   */
  std::string ToString() const override;

 protected:
  virtual Value ContainsKeyImpl(const Value& key) const;
  virtual Value ContainsValueImpl(const Value& value) const;

  // An implementation is provided, but not efficient.
  virtual Value GetImpl(const Value& key) const = 0;

  std::size_t ComputeHash() const override;
};

/** The base class for a CEL object value. */
class Object : public Container {
 public:
  /** Returns the value of a field or an error. */
  virtual Value GetMember(absl::string_view name) const = 0;
  virtual Value ContainsMember(absl::string_view name) const;

  /** They object type for this value. */
  virtual Type object_type() const = 0;

  /** Serialize the object to protobuf any. */
  virtual void To(google::protobuf::Any* value) const = 0;

  bool operator==(const Object& rhs) const;
  inline bool operator!=(const Object& rhs) const { return !(*this == rhs); }

  /**
   * Returns a canonical cel expression for the value.
   *
   * Computation may be expensive.
   */
  std::string ToString() const override;

  /**
   * Loop over members.
   *
   * Order of calls must be stable for ToString() to produce
   * a deterministic result.
   */
  virtual google::rpc::Status ForEach(
      const std::function<google::rpc::Status(absl::string_view, const Value&)>&
          call) const = 0;

  /**
   * ForEach specialization for a boolean return type.
   *
   * Iteration stops when false is returned by `call`.
   */
  template <typename T>
  ForEachReturnType<bool, T(absl::string_view, const Value&)> ForEach(
      T&& call) const;

  /**
   * ForEach specialization for a void return type.
   */
  template <typename T>
  ForEachReturnType<void, T(absl::string_view, const Value&)> ForEach(
      T&& call) const;

 protected:
  /**
   * The equal implementation.
   *
   * @param same_type a Object of the same type as the current Object.
   */
  virtual bool EqualsImpl(const Object& same_type) const = 0;
  std::size_t ComputeHash() const override;
};

Value Value::FromDouble(double value) { return Create<kDouble>(value); }
Value Value::FromString(absl::string_view value) { return Create<kStr>(value); }
Value Value::FromString(const std::string& value) { return Create<kStr>(value); }
Value Value::FromString(std::string&& value) {
  return Create<kStr>(std::move(value));
}
Value Value::FromBytes(absl::string_view value) {
  return Create<kBytes>(value);
}
Value Value::FromBytes(const std::string& value) { return Create<kBytes>(value); }
Value Value::FromBytes(std::string&& value) {
  return Create<kBytes>(std::move(value));
}
Value Value::FromDuration(absl::Duration value) {
  return Create<kDuration>(value);
}
Value Value::FromTime(absl::Time value) { return Create<kTime>(value); }

Value Value::FromEnum(NamedEnumValue value) {
  return Create<kNamedEnum>(value);
}
Value Value::FromEnum(const UnnamedEnumValue& value) {
  return Create<kUnnamedEnum>(value);
}
Value Value::FromType(BasicType value) { return Create<kBasicType>(value); }
Value Value::FromType(ObjectType value) { return Create<kObjectType>(value); }
Value Value::FromType(EnumType value) { return Create<kEnumType>(value); }
Value Value::FromType(BasicTypeValue value) {
  return Create<kBasicType>(value);
}
Value Value::FromType(const char* value) {
  return FromType(absl::string_view(value));
}
Value Value::FromType(absl::string_view full_name) {
  return FromType(Type(full_name));
}
Value Value::FromType(const std::string& full_name) {
  return FromType(Type(full_name));
}
Value Value::FromError(const google::rpc::Status& value) {
  return Create<kError>(value);
}
Value Value::FromError(google::rpc::Status&& value) {
  return Create<kError>(std::move(value));
}
Value Value::FromError(const Error& value) { return Create<kError>(value); }
Value Value::FromError(Error&& value) {
  return Create<kError>(std::move(value));
}
Value Value::FromMap(std::unique_ptr<Map> value) {
  assert(value != nullptr);
  return Create<kMap>(std::move(value));
}
Value Value::FromList(std::unique_ptr<List> value) {
  assert(value != nullptr);
  return Create<kList>(std::move(value));
}
Value Value::FromObject(std::unique_ptr<Object> value) {
  assert(value != nullptr);
  return Create<kObject>(std::move(value));
}
Value Value::ForMap(const Map* value) {
  assert(value != nullptr);
  return Create<kMapPtr>(value);
}
Value Value::ForList(const List* value) {
  assert(value != nullptr);
  return Create<kListPtr>(value);
}
Value Value::ForObject(const Object* value) {
  assert(value != nullptr);
  return Create<kObjectPtr>(value);
}
Type Value::type_value() const { return get<Type>(); }
const Map& Value::map_value() const { return get<Map>(); }
const List& Value::list_value() const { return get<List>(); }
EnumValue Value::enum_value() const { return get<EnumValue>(); }
const Object& Value::object_value() const { return get<Object>(); }
absl::string_view Value::string_value() const { return GetStr<kStr>(data_); }
absl::string_view Value::bytes_value() const { return GetStr<kBytes>(data_); }
const absl::Duration& Value::duration() const { return get<absl::Duration>(); }
const absl::Time& Value::time() const { return get<absl::Time>(); }
const Error& Value::error_value() const { return get<Error>(); }
Unknown Value::unknown_value() const { return get<Unknown>(); }

template <typename T, typename... Args>
Value Value::MakeList(Args&&... args) {
  return FromList(absl::make_unique<T>(std::forward<Args>(args)...));
}

template <typename T, typename... Args>
Value Value::MakeMap(Args&&... args) {
  return FromMap(absl::make_unique<T>(std::forward<Args>(args)...));
}

template <typename T, typename... Args>
Value Value::MakeObject(Args&&... args) {
  return FromObject(absl::make_unique<T>(std::forward<Args>(args)...));
}

bool Value::is_value() const {
  return data_.index() < kValueEnd && data_.index() != kId;
}

#define EXPR_INTERNAL_KIND_HELPER_BASE(name, type)                            \
  static Value::GetKType<Value::Kind::k##name> get(const Value* value) {      \
    return value->get<type>();                                                \
  }                                                                           \
  static Value::GetIfKType<Value::Kind::k##name> get_if(const Value* value) { \
    return value->get_if<type>();                                             \
  }                                                                           \
  template <typename T>                                                       \
  static Value From(T&& value) {                                              \
    return Value::From##name(std::forward<T>(value));                         \
  }

#define EXPR_INTERNAL_KIND_HELPER(name, type)             \
  template <>                                             \
  struct Value::KindHelper<Value::Kind::k##name> {        \
    EXPR_INTERNAL_KIND_HELPER_BASE(name, type)            \
    template <typename T>                                 \
    static Value For(T* value, const ParentRef& parent) { \
      return Value::From##name(std::forward<T>(*value));  \
    }                                                     \
  };

#define EXPR_INTERNAL_KIND_HELPER_WITH_FOR(name, type)         \
  template <>                                                  \
  struct Value::KindHelper<Value::Kind::k##name> {             \
    EXPR_INTERNAL_KIND_HELPER_BASE(name, type)                 \
    template <typename T>                                      \
    static Value For(T&& value, const ParentRef& parent) {     \
      return Value::For##name(std::forward<T>(value), parent); \
    }                                                          \
  };

EXPR_INTERNAL_KIND_HELPER(Bool, bool);
EXPR_INTERNAL_KIND_HELPER(Int, int64_t);
EXPR_INTERNAL_KIND_HELPER(UInt, uint64_t);
EXPR_INTERNAL_KIND_HELPER(Double, double);
EXPR_INTERNAL_KIND_HELPER(Type, Type);
EXPR_INTERNAL_KIND_HELPER_WITH_FOR(Map, Map);
EXPR_INTERNAL_KIND_HELPER_WITH_FOR(List, List);
EXPR_INTERNAL_KIND_HELPER_WITH_FOR(Object, Object);
EXPR_INTERNAL_KIND_HELPER(Enum, EnumValue);
EXPR_INTERNAL_KIND_HELPER(Duration, absl::Duration);
EXPR_INTERNAL_KIND_HELPER(Time, absl::Time);
EXPR_INTERNAL_KIND_HELPER(Error, Error);
EXPR_INTERNAL_KIND_HELPER(Unknown, Unknown);

template <>
struct Value::KindHelper<Value::Kind::kNull> {
  static Value::GetKType<Value::Kind::kNull> get(const Value* value) {
    return value->get<std::nullptr_t>();
  }
  static Value::GetIfKType<Value::Kind::kNull> get_if(const Value* value) {
    return value->get_if<std::nullptr_t>();
  }
  static Value From(std::nullptr_t) { return Value::NullValue(); }
  template <typename T>
  static Value For(const std::nullptr_t* value, const ParentRef& parent) {
    return Value::NullValue();
  }
};

template <>
struct Value::KindHelper<Value::Kind::kString> {
  static absl::string_view get(const Value* value) {
    return value->string_value();
  }
  static absl::optional<absl::string_view> get_if(const Value* value) {
    if (value->kind() == Value::Kind::kString) return value->string_value();
    return absl::nullopt;
  }
  template <typename T>
  static Value From(T&& value) {
    return Value::FromString(std::forward<T>(value));
  }
  template <typename T>
  static Value For(T&& value, const ParentRef& parent) {
    return Value::ForString(std::forward<T>(value), parent);
  }
};

template <>
struct Value::KindHelper<Value::Kind::kBytes> {
  static absl::string_view get(const Value* value) {
    return value->bytes_value();
  }
  static absl::optional<absl::string_view> get_if(const Value* value) {
    if (value->kind() == Value::Kind::kBytes) return value->bytes_value();
    return absl::nullopt;
  }
  template <typename T>
  static Value From(T&& value) {
    return Value::FromBytes(std::forward<T>(value));
  }
  template <typename T>
  static Value For(T&& value, const ParentRef& parent) {
    return Value::ForBytes(std::forward<T>(value), parent);
  }
};

#undef EXPR_INTERNAL_KIND_HELPER_BASE
#undef EXPR_INTERNAL_KIND_HELPER
#undef EXPR_INTERNAL_KIND_HELPER_WITH_FOR

template <typename V, typename F, typename... R>
Value::VisitType<V, F, R...> Value::visit(V&& vis, F&& value, R&&... rest) {
  return absl::visit(AdaptedVisitor<V>(std::forward<V>(vis)),
                     std::forward<F>(value).data_,
                     std::forward<R>(rest).data_...);
}

/** Applies the visitor and returns the result. */
template <typename V>
Value::VisitType<V, Value> Value::visit(V&& vis) const {
  return absl::visit(AdaptedVisitor<V>(std::forward<V>(vis)), data_);
}

template <typename T>
inline internal::BaseValue::GetType<T> Value::get() const {
  return TypeHelper<T>::get(data_);
}

template <Value::Kind K>
inline Value::GetKType<K> Value::get() const {
  return KindHelper<K>::get(this);
}

template <typename T>
inline internal::BaseValue::GetIfType<T> Value::get_if() const {
  return TypeHelper<T>::get_if(&data_);
}

template <Value::Kind K>
inline Value::GetIfKType<K> Value::get_if() const {
  return KindHelper<K>::get_if(this);
}

template <std::size_t I, typename... Args>
Value Value::Create(Args&&... args) {
  return Value(absl::in_place_index_t<I>(), std::forward<Args>(args)...);
}

inline bool Value::index_in(std::size_t start, std::size_t end) const {
  return data_.index() >= start && data_.index() < end;
}

template <typename T>
Map::ForEachReturnType<bool, T(const Value&, const Value&)> Map::ForEach(
    T&& call) const {
  return internal::IsOk(ForEach([&call](const Value& key, const Value& value) {
    return call(key, value) ? internal::OkStatus() : internal::CancelledError();
  }));
}

template <typename T>
Map::ForEachReturnType<void, T(const Value&, const Value&)> Map::ForEach(
    T&& call) const {
  ForEach([&call](const Value& key, const Value& value) {
    call(key, value);
    return internal::OkStatus();
  });
}

template <typename T>
List::ForEachReturnType<bool, T(const Value&)> List::ForEach(T&& call) const {
  return internal::IsOk(ForEach([&call](const Value& value) {
    return call(value) ? internal::OkStatus() : internal::CancelledError();
  }));
}

template <typename T>
List::ForEachReturnType<void, T(const Value&)> List::ForEach(T&& call) const {
  ForEach([&call](const Value& value) {
    call(value);
    return internal::OkStatus();
  });
}

template <typename T>
Object::ForEachReturnType<bool, T(absl::string_view, const Value&)>
Object::ForEach(T&& call) const {
  return internal::IsOk(
      ForEach([&call](absl::string_view name, const Value& value) {
        return call(name, value) ? internal::OkStatus()
                                 : internal::CancelledError();
      }));
}

template <typename T>
Object::ForEachReturnType<void, T(absl::string_view, const Value&)>
Object::ForEach(T&& call) const {
  ForEach([&call](absl::string_view name, const Value& value) {
    call(name, value);
    return internal::OkStatus();
  });
}

// Overloads for printing.
inline std::ostream& operator<<(std::ostream& os, const Value& value) {
  return os << value.ToString();
}

inline std::ostream& operator<<(std::ostream& os, const Object& value) {
  return os << value.ToString();
}

inline std::ostream& operator<<(std::ostream& os, const List& value) {
  return os << value.ToString();
}
inline std::ostream& operator<<(std::ostream& os, const Map& value) {
  return os << value.ToString();
}

}  // namespace common
}  // namespace expr
}  // namespace api
}  // namespace google

// Custom specialization of std::hash for Value.
namespace std {
template <>
struct hash<google::api::expr::common::Value> {
  typedef google::api::expr::common::Value argument_type;
  typedef std::size_t result_type;
  result_type operator()(argument_type const& value) const noexcept {
    return value.hash_code();
  }
};
}  // namespace std

#endif  // THIRD_PARTY_CEL_CPP_COMMON_CEL_VALUE_H_
