// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_BASE_TYPE_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_BASE_TYPE_FACTORY_H_

#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "base/handle.h"
#include "base/memory_manager.h"
#include "base/type.h"

namespace cel {

// TypeFactory provides member functions to get and create type implementations
// of builtin types.
//
// While TypeFactory is not final and has a virtual destructor, inheriting it is
// forbidden outside of the CEL codebase.
class TypeFactory {
 private:
  template <typename T, typename U>
  using PropagateConstT = std::conditional_t<std::is_const_v<T>, const U, U>;

  template <typename T, typename U, typename V>
  using EnableIfBaseOfT =
      std::enable_if_t<std::is_base_of_v<T, std::remove_const_t<U>>, V>;

 public:
  explicit TypeFactory(
      MemoryManager& memory_manager ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : memory_manager_(memory_manager) {}

  virtual ~TypeFactory() = default;

  TypeFactory(const TypeFactory&) = delete;
  TypeFactory& operator=(const TypeFactory&) = delete;

  Persistent<const NullType> GetNullType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const ErrorType> GetErrorType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const DynType> GetDynType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const AnyType> GetAnyType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const BoolType> GetBoolType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const IntType> GetIntType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const UintType> GetUintType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const DoubleType> GetDoubleType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const StringType> GetStringType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const BytesType> GetBytesType() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const DurationType> GetDurationType()
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Persistent<const TimestampType> GetTimestampType()
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  template <typename T, typename... Args>
  EnableIfBaseOfT<EnumType, T,
                  absl::StatusOr<Persistent<PropagateConstT<T, EnumType>>>>
  CreateEnumType(Args&&... args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base_internal::PersistentHandleFactory<PropagateConstT<
        T, EnumType>>::template Make<std::remove_const_t<T>>(memory_manager(),
                                                             std::forward<Args>(
                                                                 args)...);
  }

  absl::StatusOr<Persistent<const ListType>> CreateListType(
      const Persistent<const Type>& element) ABSL_ATTRIBUTE_LIFETIME_BOUND;

 private:
  template <typename T>
  static Persistent<const T> WrapSingletonType() {
    // This is not normal, but we treat the underlying object as having been
    // arena allocated. The only way to do this is through
    // TransientHandleFactory.
    return Persistent<const T>(
        base_internal::TransientHandleFactory<const T>::template MakeUnmanaged<
            const T>(T::Get()));
  }

  MemoryManager& memory_manager() const { return memory_manager_; }

  MemoryManager& memory_manager_;
  absl::Mutex mutex_;
  // Mapping from list element types to the list type. This allows us to cache
  // list types and avoid re-creating the same type.
  absl::flat_hash_map<Persistent<const Type>, Persistent<const ListType>>
      list_types_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPE_FACTORY_H_
