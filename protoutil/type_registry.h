#ifndef THIRD_PARTY_CEL_CPP_PROTOUTIL_TYPE_REGISTRY_H_
#define THIRD_PARTY_CEL_CPP_PROTOUTIL_TYPE_REGISTRY_H_

#include <memory>

#include "absl/container/node_hash_map.h"
#include "common/parent_ref.h"
#include "common/type.h"
#include "common/value.h"
#include "internal/cast.h"

namespace google {
namespace api {
namespace expr {
namespace protoutil {

/**
 * A registry for adapting `google::protobuf::Message` to `expr::Value`.
 *
 * All messages have a default implementation that matches typical protobuf
 * codegen.
 *
 * Constructor functions and default values can be registered to customize
 * the behavior of specific protobuf message and enum types.
 *
 * For messages, several different types of constructor arguments are supported:
 *  (1) `const proto2:Message &` for copy constructors.
 *  (2) `google::protobuf::Message &&` for move constructors.
 *  (3) `std::unique_ptr` for owned ptr constructors.
 *  (4) `const google::protobuf::Message*' for unowned pointer constructors. In this case
 *  the argument is guaranteed out live any `Value` returned. Typically used
 *  for Arena allocated protobufs.
 *  (5) 'const proto2:Message*, const RefProvider&` for 'view' constructors. In
 *  this case the message is guaranteed to live at least as long as any
 *  `ValueRef` retrieved from the provider. Used to support 'view'
 *  implementations when elements of a `Container` are accessed.
 *
 * Helper functions are provided to register functions and classes that accept
 * concrete message types. For example:
 *
 *     // Register a (1)-style constructor function.
 *     GOOGLE_CHECK(reg.RegisterConstructor<google::type::Money>(
 *         [](const google::type::Money& value) {
 *           ...
 *         }));
 *
 *     class Money : public google::api::expr::Object {
 *      public:
 *       Money(const google::type::Money& value);
 *       Money(google::type::Money&& value);
 *     };
 *     // Register both Money constructors and set the default value to 'null'.
 *     GOOGLE_CHECK(2 == reg.RegisterClass<google::type::Money, Money>();
 *
 * Not all constructor types need to be registered:
 *  - If the underlying protobuf message is never needed by the returned
 *  Value, only a (1)-style constructor need be registered.
 *  - If some of the protobuf message fields can be used directly by the
 *  returned Value, (2) and (4)-style constructors can be registered to
 *  reduce memory copying.
 *  - If the entire protobuf message can be used directly by the returned Value,
 *  (3) and (4)-style constructors can be registered to reduce memory copying.
 *  - Register a (5)-style constructor if containers should lazily construct
 *  Values on access.
 *
 * At least one of (1), (2), and (3) must be registered for every customized
 * type. If this is not the case, and error may be returned instead of a value.
 *
 * If a default value is not registered for a customized type, 'null' will be
 * used.
 *
 * Callbacks are tried in the following order:
 * (1) -> (3) -> (2)
 * (2) -> (3) -> (1)
 * (3) -> (2) -> (1)
 * (4) -> (1) -> (3) -> (2)
 * (5) -> (1) -> (3) -> (2)
 *
 * This class must live longer than any value created from it.
 */
class TypeRegistry {
 public:
  /** Set the default value, returned when a protobuf field is unset. */
  bool RegisterDefault(const common::ObjectType& object_type,
                       const common::Value& default_value);

  /** Set the default value, returned when a protobuf field is unset. */
  template <typename ProtoType>
  bool RegisterDefault(const common::Value& default_value) {
    return RegisterDefault(common::ObjectType(ProtoType::descriptor()),
                           default_value);
  }

  /** Register a copy constructor callable for the given object_type. */
  bool RegisterConstructor(
      const common::ObjectType& object_type,
      std::function<common::Value(const google::protobuf::Message&)> from_ctor);

  /** Register a move constructor callable for the given object_type. */
  bool RegisterConstructor(
      const common::ObjectType& object_type,
      std::function<common::Value(google::protobuf::Message&&)> from_ctor);

  /** Register an owned ptr constructor callable for the given object_type. */
  bool RegisterConstructor(
      const common::ObjectType& object_type,
      std::function<common::Value(std::unique_ptr<google::protobuf::Message>)> from_ctor);

  /** Register an unowned ptr constructor callable for the given object_type. */
  bool RegisterConstructor(
      const common::ObjectType& object_type,
      std::function<common::Value(const google::protobuf::Message*)> for_ctor);

  /** Register a view constructor callable for the given object_type when
   * owned by a parent container. */
  bool RegisterConstructor(
      const common::ObjectType& object_type,
      std::function<common::Value(const google::protobuf::Message*,
                                  const common::RefProvider&)>
          for_ctor);

  /** Register a copy constructor callable for the given enum_type. */
  bool RegisterConstructor(
      const common::EnumType& enum_type,
      std::function<common::Value(common::EnumType, int32_t)> from_ctor);

  /**
   * Register a constructor for a concrete protobuf message.
   *
   * The signature of 'call' is used to determin which type of constructor
   * callable it is.
   *
   * @tparam ProtoType the concrete protobuf message class.
   * @tparam C the constructor type. Templatized to allow inline of lambda
   * invocation.
   * @returns false if a constructor was previously registered.
   */
  template <typename ProtoType, typename C>
  bool RegisterConstructor(C&& call);

  /**
   * Register all applicable constructors of implementation class.
   *
   * Default values is set to 'null' no other default has been registered.
   *
   * @tparam ProtoType the concrete protobuf message class.
   * @tparam Impl the implementation class to register.
   * @returns The number of constructors successfully registered.
   */
  template <typename ProtoType, typename Impl>
  std::size_t RegisterClass();

  /**
   * Register all applicable constructors of implementation class and
   * set the default value.
   *
   * @tparam ProtoType the concrete protobuf message class.
   * @tparam Impl the implementation class to register.
   * @returns The number of constructors successfully registered.
   */
  template <typename ProtoType, typename Impl>
  std::size_t RegisterClass(const common::Value& default_value) {
    RegisterDefault(common::ObjectType::For<ProtoType>(), default_value);
    return RegisterClass<ProtoType, Impl>();
  }

  /** Return the default value for the given default_instance */
  common::Value GetDefault(const google::protobuf::Message* default_msg) const;

  /** Return a value created from the given message. */
  common::Value ValueFrom(const google::protobuf::Message& value) const;
  /** Return a value created from the given message. */
  common::Value ValueFrom(google::protobuf::Message&& value) const;
  /** Return a value created from the given message. */
  common::Value ValueFrom(std::unique_ptr<google::protobuf::Message> value) const;
  /** Return a value created for the given message. */
  common::Value ValueFor(const google::protobuf::Message* value,
                         common::ParentRef parent = common::NoParent()) const;

  /** Return a value crated from the given enum value */
  common::Value ValueFrom(const common::EnumType& type, int32_t value) const;

 private:
  struct ObjectRegistryEntry {
    common::Value default_value = common::Value::FromUnknown(common::Id(-1));
    std::function<common::Value(const google::protobuf::Message&)> from_ctor;
    std::function<common::Value(google::protobuf::Message&&)> from_move_ctor;
    std::function<common::Value(std::unique_ptr<google::protobuf::Message>)>
        from_ptr_ctor;
    std::function<common::Value(const google::protobuf::Message*)> for_ctor;
    std::function<common::Value(const google::protobuf::Message*,
                                const common::RefProvider&)>
        for_pnt_ctor;
  };

  struct EnumRegistryEntry {
    std::function<common::Value(common::EnumType, int32_t)> from_ctor;
  };

  absl::node_hash_map<common::ObjectType, ObjectRegistryEntry> object_registry_;
  absl::node_hash_map<common::EnumType, EnumRegistryEntry> enum_registry_;

  common::Value ValueFromUnregistered(
      std::unique_ptr<google::protobuf::Message> value) const;
  common::Value ValueForUnregistered(
      const google::protobuf::Message* value,
      common::RefProvider parent = common::NoParent()) const;
  common::Value ValueFromAny(const google::protobuf::Any& value) const;

  ObjectRegistryEntry GetCalls(const common::ObjectType& type) const;
};

namespace type_registry_internal {
using internal::general;
using internal::inst_of;
using internal::specialize_for;
using internal::specialize_ifd;
using internal::static_down_cast;

// Wrap a callable, casting the first argument from M to T.
template <typename C, typename T, typename M, typename... Args>
std::function<common::Value(M, Args...)> WrapCall(C call) {
  return [call](M value, Args&&... args) {
    return call(static_down_cast<T>(std::forward<M>(value)),
                std::forward<Args>(args)...);
  };
}

// Wrap a constructor, casting the first argument from M to T.
template <typename C, typename T, typename M, typename... Args>
std::function<common::Value(M, Args...)> WrapCtor(
    specialize_for<decltype(C(inst_of<T>(), inst_of<Args>()...))>) {
  return [](M value, Args&&... args) {
    return common::Value::MakeObject<C>(
        static_down_cast<T, M>(std::forward<M>(value)),
        std::forward<Args>(args)...);
  };
}

// No constructor specialization found, return nullptr.
template <typename C, typename T, typename M, typename... Args>
std::function<common::Value(M, Args...)> WrapCtor(general) {
  return nullptr;
}

template <typename C, typename T, typename... Args>
using reg_if =
    specialize_ifd<bool,
                   decltype(inst_of<C>()(inst_of<T>(), inst_of<Args>()...))>;

template <typename ProtoType, typename C>
reg_if<C, const ProtoType&> Register(TypeRegistry* registry, C call) {
  return registry->RegisterConstructor(
      common::ObjectType(ProtoType::descriptor()),
      WrapCall<C, const ProtoType&, const google::protobuf::Message&>(call));
}

template <typename ProtoType, typename C>
reg_if<C, std::unique_ptr<ProtoType>> Register(TypeRegistry* registry, C call) {
  return registry->RegisterConstructor(
      common::ObjectType(ProtoType::descriptor()),
      WrapCall<C, std::unique_ptr<ProtoType>, std::unique_ptr<google::protobuf::Message>>(
          call));
}

template <typename ProtoType, typename C>
reg_if<C, const ProtoType*> Register(TypeRegistry* registry, C call) {
  return registry->RegisterConstructor(
      common::ObjectType(ProtoType::descriptor()),
      WrapCall<C, const ProtoType*, const google::protobuf::Message*>(call));
}

template <typename ProtoType, typename C>
reg_if<C, const ProtoType*, const common::RefProvider&> Register(
    TypeRegistry* registry, C call) {
  return registry->RegisterConstructor(
      common::ObjectType(ProtoType::descriptor()),
      WrapCall<C, const ProtoType*, const google::protobuf::Message*,
               const common::RefProvider&>(call));
}

}  // namespace type_registry_internal

template <typename ProtoType, typename C>
bool TypeRegistry::RegisterConstructor(C&& call) {
  return type_registry_internal::Register<ProtoType>(this,
                                                     std::forward<C>(call));
}

template <typename ProtoType, typename Impl>
std::size_t TypeRegistry::RegisterClass() {
  static_assert(!std::is_abstract<Impl>::value, "class cannot be abstract");
  auto from_ctor = type_registry_internal::WrapCtor<Impl, const ProtoType&,
                                                    const google::protobuf::Message&>(
      internal::specialize());
  auto from_move_ctor =
      type_registry_internal::WrapCtor<Impl, ProtoType&&, google::protobuf::Message&&>(
          internal::specialize());
  auto from_ptr_ctor =
      type_registry_internal::WrapCtor<Impl, std::unique_ptr<ProtoType>,
                                       std::unique_ptr<google::protobuf::Message>>(
          internal::specialize());
  auto for_ctor = type_registry_internal::WrapCtor<Impl, const ProtoType*,
                                                   const google::protobuf::Message*>(
      internal::specialize());
  auto for_pnt_ctor =
      type_registry_internal::WrapCtor<Impl, const ProtoType*,
                                       const google::protobuf::Message*,
                                       const common::RefProvider&>(
          internal::specialize());
  common::ObjectType type(ProtoType::descriptor());
  std::size_t successes = 0;
  if (from_ctor != nullptr && RegisterConstructor(type, from_ctor)) {
    ++successes;
  }
  if (from_move_ctor != nullptr && RegisterConstructor(type, from_move_ctor)) {
    ++successes;
  }
  if (from_ptr_ctor != nullptr && RegisterConstructor(type, from_ptr_ctor)) {
    ++successes;
  }
  if (for_ctor != nullptr && RegisterConstructor(type, for_ctor)) {
    ++successes;
  }
  if (for_pnt_ctor != nullptr && RegisterConstructor(type, for_pnt_ctor)) {
    ++successes;
  }
  return successes;
}

}  // namespace protoutil
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_COMMON_OBJECT_REGISTRY_H_
