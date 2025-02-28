// Copyright 2024 Google LLC
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

#include "common/internal/byte_string.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <tuple>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/hash/hash.h"
#include "absl/log/absl_check.h"
#include "absl/strings/cord.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/allocator.h"
#include "common/internal/metadata.h"
#include "common/internal/reference_count.h"
#include "common/memory.h"
#include "google/protobuf/arena.h"

namespace cel::common_internal {

namespace {

char* CopyCordToArray(const absl::Cord& cord, char* data) {
  for (auto chunk : cord.Chunks()) {
    std::memcpy(data, chunk.data(), chunk.size());
    data += chunk.size();
  }
  return data;
}

template <typename T>
T ConsumeAndDestroy(T& object) {
  T consumed = std::move(object);
  object.~T();  // NOLINT(bugprone-use-after-move)
  return consumed;
}

}  // namespace

ByteString::ByteString(Allocator<> allocator, absl::string_view string) {
  ABSL_DCHECK_LE(string.size(), max_size());
  auto* arena = allocator.arena();
  if (string.size() <= kSmallByteStringCapacity) {
    SetSmall(arena, string);
  } else {
    SetMedium(arena, string);
  }
}

ByteString::ByteString(Allocator<> allocator, const std::string& string) {
  ABSL_DCHECK_LE(string.size(), max_size());
  auto* arena = allocator.arena();
  if (string.size() <= kSmallByteStringCapacity) {
    SetSmall(arena, string);
  } else {
    SetMedium(arena, string);
  }
}

ByteString::ByteString(Allocator<> allocator, std::string&& string) {
  ABSL_DCHECK_LE(string.size(), max_size());
  auto* arena = allocator.arena();
  if (string.size() <= kSmallByteStringCapacity) {
    SetSmall(arena, string);
  } else {
    SetMedium(arena, std::move(string));
  }
}

ByteString::ByteString(Allocator<> allocator, const absl::Cord& cord) {
  ABSL_DCHECK_LE(cord.size(), max_size());
  auto* arena = allocator.arena();
  if (cord.size() <= kSmallByteStringCapacity) {
    SetSmall(arena, cord);
  } else if (arena != nullptr) {
    SetMedium(arena, cord);
  } else {
    SetLarge(cord);
  }
}

ByteString ByteString::Borrowed(Borrower borrower, absl::string_view string) {
  ABSL_DCHECK(borrower != Borrower::None()) << "Borrowing from Owner::None()";
  auto* arena = borrower.arena();
  if (string.size() <= kSmallByteStringCapacity || arena != nullptr) {
    return ByteString(arena, string);
  }
  const auto* refcount = BorrowerRelease(borrower);
  // A nullptr refcount indicates somebody called us to borrow something that
  // has no owner. If this is the case, we fallback to assuming operator
  // new/delete and convert it to a reference count.
  if (refcount == nullptr) {
    std::tie(refcount, string) = MakeReferenceCountedString(string);
  } else {
    StrongRef(*refcount);
  }
  return ByteString(refcount, string);
}

ByteString ByteString::Borrowed(Borrower borrower, const absl::Cord& cord) {
  ABSL_DCHECK(borrower != Borrower::None()) << "Borrowing from Owner::None()";
  return ByteString(borrower.arena(), cord);
}

ByteString::ByteString(absl::Nonnull<const ReferenceCount*> refcount,
                       absl::string_view string) {
  ABSL_DCHECK_LE(string.size(), max_size());
  SetMedium(string, reinterpret_cast<uintptr_t>(refcount) |
                        kMetadataOwnerReferenceCountBit);
}

absl::Nullable<google::protobuf::Arena*> ByteString::GetArena() const {
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      return GetSmallArena();
    case ByteStringKind::kMedium:
      return GetMediumArena();
    case ByteStringKind::kLarge:
      return nullptr;
  }
}

bool ByteString::empty() const {
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      return rep_.small.size == 0;
    case ByteStringKind::kMedium:
      return rep_.medium.size == 0;
    case ByteStringKind::kLarge:
      return GetLarge().empty();
  }
}

size_t ByteString::size() const {
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      return rep_.small.size;
    case ByteStringKind::kMedium:
      return rep_.medium.size;
    case ByteStringKind::kLarge:
      return GetLarge().size();
  }
}

absl::string_view ByteString::Flatten() {
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      return GetSmall();
    case ByteStringKind::kMedium:
      return GetMedium();
    case ByteStringKind::kLarge:
      return GetLarge().Flatten();
  }
}

absl::optional<absl::string_view> ByteString::TryFlat() const {
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      return GetSmall();
    case ByteStringKind::kMedium:
      return GetMedium();
    case ByteStringKind::kLarge:
      return GetLarge().TryFlat();
  }
}

void ByteString::RemovePrefix(size_t n) {
  ABSL_DCHECK_LE(n, size());
  if (n == 0) {
    return;
  }
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      std::memmove(rep_.small.data, rep_.small.data + n, rep_.small.size - n);
      rep_.small.size -= n;
      break;
    case ByteStringKind::kMedium:
      rep_.medium.data += n;
      rep_.medium.size -= n;
      if (rep_.medium.size <= kSmallByteStringCapacity) {
        const auto* refcount = GetMediumReferenceCount();
        SetSmall(GetMediumArena(), GetMedium());
        StrongUnref(refcount);
      }
      break;
    case ByteStringKind::kLarge: {
      auto& large = GetLarge();
      const auto large_size = large.size();
      const auto new_large_pos = n;
      const auto new_large_size = large_size - n;
      large = large.Subcord(new_large_pos, new_large_size);
      if (new_large_size <= kSmallByteStringCapacity) {
        auto large_copy = std::move(large);
        DestroyLarge();
        SetSmall(nullptr, large_copy);
      }
    } break;
  }
}

void ByteString::RemoveSuffix(size_t n) {
  ABSL_DCHECK_LE(n, size());
  if (n == 0) {
    return;
  }
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      rep_.small.size -= n;
      break;
    case ByteStringKind::kMedium:
      rep_.medium.size -= n;
      if (rep_.medium.size <= kSmallByteStringCapacity) {
        const auto* refcount = GetMediumReferenceCount();
        SetSmall(GetMediumArena(), GetMedium());
        StrongUnref(refcount);
      }
      break;
    case ByteStringKind::kLarge: {
      auto& large = GetLarge();
      const auto large_size = large.size();
      const auto new_large_pos = 0;
      const auto new_large_size = large_size - n;
      large = large.Subcord(new_large_pos, new_large_size);
      if (new_large_size <= kSmallByteStringCapacity) {
        auto large_copy = std::move(large);
        DestroyLarge();
        SetSmall(nullptr, large_copy);
      }
    } break;
  }
}

std::string ByteString::ToString() const {
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      return std::string(GetSmall());
    case ByteStringKind::kMedium:
      return std::string(GetMedium());
    case ByteStringKind::kLarge:
      return static_cast<std::string>(GetLarge());
  }
}

void ByteString::CopyToString(absl::Nonnull<std::string*> out) const {
  ABSL_DCHECK(out != nullptr);

  switch (GetKind()) {
    case ByteStringKind::kSmall:
      out->assign(GetSmall());
      break;
    case ByteStringKind::kMedium:
      out->assign(GetMedium());
      break;
    case ByteStringKind::kLarge:
      absl::CopyCordToString(GetLarge(), out);
      break;
  }
}

void ByteString::AppendToString(absl::Nonnull<std::string*> out) const {
  ABSL_DCHECK(out != nullptr);

  switch (GetKind()) {
    case ByteStringKind::kSmall:
      out->append(GetSmall());
      break;
    case ByteStringKind::kMedium:
      out->append(GetMedium());
      break;
    case ByteStringKind::kLarge:
      absl::AppendCordToString(GetLarge(), out);
      break;
  }
}

namespace {

struct ReferenceCountReleaser {
  absl::Nonnull<const ReferenceCount*> refcount;

  void operator()() const { StrongUnref(*refcount); }
};

}  // namespace

absl::Cord ByteString::ToCord() const& {
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      return absl::Cord(GetSmall());
    case ByteStringKind::kMedium: {
      const auto* refcount = GetMediumReferenceCount();
      if (refcount != nullptr) {
        StrongRef(*refcount);
        return absl::MakeCordFromExternal(GetMedium(),
                                          ReferenceCountReleaser{refcount});
      }
      return absl::Cord(GetMedium());
    }
    case ByteStringKind::kLarge:
      return GetLarge();
  }
}

absl::Cord ByteString::ToCord() && {
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      return absl::Cord(GetSmall());
    case ByteStringKind::kMedium: {
      const auto* refcount = GetMediumReferenceCount();
      if (refcount != nullptr) {
        auto medium = GetMedium();
        SetSmallEmpty(nullptr);
        return absl::MakeCordFromExternal(medium,
                                          ReferenceCountReleaser{refcount});
      }
      return absl::Cord(GetMedium());
    }
    case ByteStringKind::kLarge:
      return GetLarge();
  }
}

void ByteString::CopyToCord(absl::Nonnull<absl::Cord*> out) const {
  ABSL_DCHECK(out != nullptr);

  switch (GetKind()) {
    case ByteStringKind::kSmall:
      *out = absl::Cord(GetSmall());
      break;
    case ByteStringKind::kMedium: {
      const auto* refcount = GetMediumReferenceCount();
      if (refcount != nullptr) {
        StrongRef(*refcount);
        *out = absl::MakeCordFromExternal(GetMedium(),
                                          ReferenceCountReleaser{refcount});
      } else {
        *out = absl::Cord(GetMedium());
      }
    } break;
    case ByteStringKind::kLarge:
      *out = GetLarge();
      break;
  }
}

void ByteString::AppendToCord(absl::Nonnull<absl::Cord*> out) const {
  ABSL_DCHECK(out != nullptr);

  switch (GetKind()) {
    case ByteStringKind::kSmall:
      out->Append(GetSmall());
      break;
    case ByteStringKind::kMedium: {
      const auto* refcount = GetMediumReferenceCount();
      if (refcount != nullptr) {
        StrongRef(*refcount);
        out->Append(absl::MakeCordFromExternal(
            GetMedium(), ReferenceCountReleaser{refcount}));
      } else {
        out->Append(GetMedium());
      }
    } break;
    case ByteStringKind::kLarge:
      out->Append(GetLarge());
      break;
  }
}

absl::string_view ByteString::ToStringView(
    absl::Nonnull<std::string*> scratch) const {
  ABSL_DCHECK(scratch != nullptr);

  switch (GetKind()) {
    case ByteStringKind::kSmall:
      return GetSmall();
    case ByteStringKind::kMedium:
      return GetMedium();
    case ByteStringKind::kLarge:
      if (auto flat = GetLarge().TryFlat(); flat) {
        return *flat;
      }
      absl::CopyCordToString(GetLarge(), scratch);
      return absl::string_view(*scratch);
  }
}

absl::string_view ByteString::AsStringView() const {
  const ByteStringKind kind = GetKind();
  ABSL_CHECK(kind == ByteStringKind::kSmall ||  // Crash OK
             kind == ByteStringKind::kMedium);
  switch (kind) {
    case ByteStringKind::kSmall:
      return GetSmall();
    case ByteStringKind::kMedium:
      return GetMedium();
    case ByteStringKind::kLarge:
      ABSL_UNREACHABLE();
  }
}

absl::Nullable<google::protobuf::Arena*> ByteString::GetMediumArena(
    const MediumByteStringRep& rep) {
  if ((rep.owner & kMetadataOwnerBits) == kMetadataOwnerArenaBit) {
    return reinterpret_cast<google::protobuf::Arena*>(rep.owner &
                                            kMetadataOwnerPointerMask);
  }
  return nullptr;
}

absl::Nullable<const ReferenceCount*> ByteString::GetMediumReferenceCount(
    const MediumByteStringRep& rep) {
  if ((rep.owner & kMetadataOwnerBits) == kMetadataOwnerReferenceCountBit) {
    return reinterpret_cast<const ReferenceCount*>(rep.owner &
                                                   kMetadataOwnerPointerMask);
  }
  return nullptr;
}

void ByteString::Construct(const ByteString& other,
                           absl::optional<Allocator<>> allocator) {
  switch (other.GetKind()) {
    case ByteStringKind::kSmall:
      rep_.small = other.rep_.small;
      if (allocator.has_value()) {
        rep_.small.arena = allocator->arena();
      }
      break;
    case ByteStringKind::kMedium:
      if (allocator.has_value() &&
          allocator->arena() != other.GetMediumArena()) {
        SetMedium(allocator->arena(), other.GetMedium());
      } else {
        rep_.medium = other.rep_.medium;
        StrongRef(GetMediumReferenceCount());
      }
      break;
    case ByteStringKind::kLarge:
      if (allocator.has_value() && allocator->arena() != nullptr) {
        SetMedium(allocator->arena(), other.GetLarge());
      } else {
        SetLarge(other.GetLarge());
      }
      break;
  }
}

void ByteString::Construct(ByteStringView other,
                           absl::optional<Allocator<>> allocator) {
  switch (other.GetKind()) {
    case ByteStringViewKind::kString: {
      absl::string_view string = other.GetString();
      if (string.size() <= kSmallByteStringCapacity) {
        absl::Nullable<google::protobuf::Arena*> arena = other.GetStringArena();
        if (allocator.has_value()) {
          arena = allocator->arena();
        }
        SetSmall(arena, string);
      } else {
        uintptr_t owner = other.GetStringOwner();
        if (owner == 0) {
          SetMedium(allocator.value_or(NewDeleteAllocator()).arena(), string);
        } else {
          if (allocator.has_value() &&
              allocator->arena() != other.GetStringArena()) {
            SetMedium(allocator->arena(), string);
          } else {
            SetMedium(string, owner);
            StrongRef(GetMediumReferenceCount());
          }
        }
      }
    } break;
    case ByteStringViewKind::kCord:
      if (allocator.has_value() && allocator->arena() != nullptr) {
        SetMedium(allocator->arena(), other.GetSubcord());
      } else {
        absl::Cord cord = other.GetSubcord();
        if (cord.size() <= kSmallByteStringCapacity) {
          SetSmall(allocator.value_or(NewDeleteAllocator()).arena(), cord);
        } else {
          SetLarge(std::move(cord));
        }
      }
      break;
  }
}

void ByteString::Construct(ByteString& other,
                           absl::optional<Allocator<>> allocator) {
  switch (other.GetKind()) {
    case ByteStringKind::kSmall:
      rep_.small = other.rep_.small;
      if (allocator.has_value()) {
        rep_.small.arena = allocator->arena();
      }
      break;
    case ByteStringKind::kMedium:
      if (allocator.has_value() &&
          allocator->arena() != other.GetMediumArena()) {
        SetMedium(allocator->arena(), other.GetMedium());
      } else {
        rep_.medium = other.rep_.medium;
        other.rep_.medium.owner = 0;
      }
      break;
    case ByteStringKind::kLarge:
      if (allocator.has_value() && allocator->arena() != nullptr) {
        SetMedium(allocator->arena(), other.GetLarge());
      } else {
        SetLarge(std::move(other.GetLarge()));
      }
      break;
  }
}

void ByteString::CopyFrom(const ByteString& other) {
  ABSL_DCHECK_NE(&other, this);

  switch (other.GetKind()) {
    case ByteStringKind::kSmall:
      switch (GetKind()) {
        case ByteStringKind::kSmall:
          break;
        case ByteStringKind::kMedium:
          DestroyMedium();
          break;
        case ByteStringKind::kLarge:
          DestroyLarge();
          break;
      }
      rep_.small = other.rep_.small;
      break;
    case ByteStringKind::kMedium:
      switch (GetKind()) {
        case ByteStringKind::kSmall:
          rep_.medium = other.rep_.medium;
          StrongRef(GetMediumReferenceCount());
          break;
        case ByteStringKind::kMedium:
          StrongRef(other.GetMediumReferenceCount());
          DestroyMedium();
          rep_.medium = other.rep_.medium;
          break;
        case ByteStringKind::kLarge:
          DestroyLarge();
          rep_.medium = other.rep_.medium;
          StrongRef(GetMediumReferenceCount());
          break;
      }
      break;
    case ByteStringKind::kLarge:
      switch (GetKind()) {
        case ByteStringKind::kSmall:
          SetLarge(other.GetLarge());
          break;
        case ByteStringKind::kMedium:
          DestroyMedium();
          SetLarge(other.GetLarge());
          break;
        case ByteStringKind::kLarge:
          GetLarge() = other.GetLarge();
          break;
      }
      break;
  }
}

void ByteString::CopyFrom(ByteStringView other) {
  switch (other.GetKind()) {
    case ByteStringViewKind::kString: {
      absl::string_view string = other.GetString();
      switch (GetKind()) {
        case ByteStringKind::kSmall:
          if (string.size() <= kSmallByteStringCapacity) {
            SetSmall(other.GetStringArena(), string);
          } else {
            uintptr_t owner = other.GetStringOwner();
            if (owner == 0) {
              // No owner, we have to force a reference counted copy.
              SetMedium(nullptr, string);
            } else {
              SetMedium(string, owner);
              StrongRef(GetMediumReferenceCount());
            }
          }
          break;
        case ByteStringKind::kMedium:
          if (string.size() <= kSmallByteStringCapacity) {
            absl::Nullable<google::protobuf::Arena*> arena = other.GetStringArena();
            DestroyMedium();
            SetSmall(arena, string);
          } else {
            uintptr_t owner = other.GetStringOwner();
            if (owner == 0) {
              DestroyMedium();
              // No owner, we have to force a reference counted copy.
              SetMedium(nullptr, string);
            } else {
              StrongRef(other.GetStringReferenceCount());
              DestroyMedium();
              SetMedium(string, owner);
            }
          }
          break;
        case ByteStringKind::kLarge:
          DestroyLarge();
          if (string.size() <= kSmallByteStringCapacity) {
            SetSmall(other.GetStringArena(), string);
          } else {
            uintptr_t owner = other.GetStringOwner();
            if (owner == 0) {
              // No owner, we have to force a reference counted copy.
              SetMedium(nullptr, string);
            } else {
              SetMedium(string, owner);
              StrongRef(GetMediumReferenceCount());
            }
          }
          break;
      }
    } break;
    case ByteStringViewKind::kCord: {
      absl::Cord cord = other.GetSubcord();
      switch (GetKind()) {
        case ByteStringKind::kSmall:
          if (cord.size() <= kSmallByteStringCapacity) {
            SetSmall(nullptr, cord);
          } else {
            SetLarge(std::move(cord));
          }
          break;
        case ByteStringKind::kMedium:
          DestroyMedium();
          if (cord.size() <= kSmallByteStringCapacity) {
            SetSmall(nullptr, cord);
          } else {
            SetLarge(std::move(cord));
          }
          break;
        case ByteStringKind::kLarge:
          if (cord.size() <= kSmallByteStringCapacity) {
            DestroyLarge();
            SetSmall(nullptr, cord);
          } else {
            GetLarge() = other.GetSubcord();
          }
          break;
      }
    } break;
  }
}

void ByteString::MoveFrom(ByteString& other) {
  ABSL_DCHECK_NE(&other, this);

  switch (other.GetKind()) {
    case ByteStringKind::kSmall:
      switch (GetKind()) {
        case ByteStringKind::kSmall:
          break;
        case ByteStringKind::kMedium:
          DestroyMedium();
          break;
        case ByteStringKind::kLarge:
          DestroyLarge();
          break;
      }
      rep_.small = other.rep_.small;
      break;
    case ByteStringKind::kMedium:
      switch (GetKind()) {
        case ByteStringKind::kSmall:
          rep_.medium = other.rep_.medium;
          break;
        case ByteStringKind::kMedium:
          DestroyMedium();
          rep_.medium = other.rep_.medium;
          break;
        case ByteStringKind::kLarge:
          DestroyLarge();
          rep_.medium = other.rep_.medium;
          break;
      }
      other.rep_.medium.owner = 0;
      break;
    case ByteStringKind::kLarge:
      switch (GetKind()) {
        case ByteStringKind::kSmall:
          SetLarge(std::move(other.GetLarge()));
          break;
        case ByteStringKind::kMedium:
          DestroyMedium();
          SetLarge(std::move(other.GetLarge()));
          break;
        case ByteStringKind::kLarge:
          GetLarge() = std::move(other.GetLarge());
          break;
      }
      break;
  }
}

ByteString ByteString::Clone(Allocator<> allocator) const {
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      return ByteString(allocator, GetSmall());
    case ByteStringKind::kMedium: {
      absl::Nullable<google::protobuf::Arena*> arena = allocator.arena();
      absl::Nullable<google::protobuf::Arena*> other_arena = GetMediumArena();
      if (arena != nullptr) {
        if (arena == other_arena) {
          return *this;
        }
        return ByteString(allocator, GetMedium());
      }
      if (other_arena != nullptr) {
        return ByteString(allocator, GetMedium());
      }
      return *this;
    }
    case ByteStringKind::kLarge:
      return ByteString(allocator, GetLarge());
  }
}

void ByteString::HashValue(absl::HashState state) const {
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      absl::HashState::combine(std::move(state), GetSmall());
      break;
    case ByteStringKind::kMedium:
      absl::HashState::combine(std::move(state), GetMedium());
      break;
    case ByteStringKind::kLarge:
      absl::HashState::combine(std::move(state), GetLarge());
      break;
  }
}

void ByteString::Swap(ByteString& other) {
  ABSL_DCHECK_NE(&other, this);
  using std::swap;

  switch (other.GetKind()) {
    case ByteStringKind::kSmall:
      switch (GetKind()) {
        case ByteStringKind::kSmall:
          // small <=> small
          swap(rep_.small, other.rep_.small);
          break;
        case ByteStringKind::kMedium:
          // medium <=> small
          swap(rep_, other.rep_);
          break;
        case ByteStringKind::kLarge: {
          absl::Cord cord = std::move(GetLarge());
          DestroyLarge();
          rep_ = other.rep_;
          other.SetLarge(std::move(cord));
        } break;
      }
      break;
    case ByteStringKind::kMedium:
      switch (GetKind()) {
        case ByteStringKind::kSmall:
          swap(rep_, other.rep_);
          break;
        case ByteStringKind::kMedium:
          swap(rep_.medium, other.rep_.medium);
          break;
        case ByteStringKind::kLarge: {
          absl::Cord cord = std::move(GetLarge());
          DestroyLarge();
          rep_ = other.rep_;
          other.SetLarge(std::move(cord));
        } break;
      }
      break;
    case ByteStringKind::kLarge:
      switch (GetKind()) {
        case ByteStringKind::kSmall: {
          absl::Cord cord = std::move(other.GetLarge());
          other.DestroyLarge();
          other.rep_.small = rep_.small;
          SetLarge(std::move(cord));
        } break;
        case ByteStringKind::kMedium: {
          absl::Cord cord = std::move(other.GetLarge());
          other.DestroyLarge();
          other.rep_.medium = rep_.medium;
          SetLarge(std::move(cord));
        } break;
        case ByteStringKind::kLarge:
          swap(GetLarge(), other.GetLarge());
          break;
      }
      break;
  }
}

void ByteString::Destroy() {
  switch (GetKind()) {
    case ByteStringKind::kSmall:
      break;
    case ByteStringKind::kMedium:
      DestroyMedium();
      break;
    case ByteStringKind::kLarge:
      DestroyLarge();
      break;
  }
}

void ByteString::SetSmall(absl::Nullable<google::protobuf::Arena*> arena,
                          absl::string_view string) {
  ABSL_DCHECK_LE(string.size(), kSmallByteStringCapacity);
  rep_.header.kind = ByteStringKind::kSmall;
  rep_.small.size = string.size();
  rep_.small.arena = arena;
  std::memcpy(rep_.small.data, string.data(), rep_.small.size);
}

void ByteString::SetSmall(absl::Nullable<google::protobuf::Arena*> arena,
                          const absl::Cord& cord) {
  ABSL_DCHECK_LE(cord.size(), kSmallByteStringCapacity);
  rep_.header.kind = ByteStringKind::kSmall;
  rep_.small.size = cord.size();
  rep_.small.arena = arena;
  (CopyCordToArray)(cord, rep_.small.data);
}

void ByteString::SetMedium(absl::Nullable<google::protobuf::Arena*> arena,
                           absl::string_view string) {
  ABSL_DCHECK_GT(string.size(), kSmallByteStringCapacity);
  rep_.header.kind = ByteStringKind::kMedium;
  rep_.medium.size = string.size();
  if (arena != nullptr) {
    char* data = static_cast<char*>(
        arena->AllocateAligned(rep_.medium.size, alignof(char)));
    std::memcpy(data, string.data(), rep_.medium.size);
    rep_.medium.data = data;
    rep_.medium.owner =
        reinterpret_cast<uintptr_t>(arena) | kMetadataOwnerArenaBit;
  } else {
    auto pair = MakeReferenceCountedString(string);
    rep_.medium.data = pair.second.data();
    rep_.medium.owner = reinterpret_cast<uintptr_t>(pair.first) |
                        kMetadataOwnerReferenceCountBit;
  }
}

void ByteString::SetMedium(absl::Nullable<google::protobuf::Arena*> arena,
                           std::string&& string) {
  ABSL_DCHECK_GT(string.size(), kSmallByteStringCapacity);
  rep_.header.kind = ByteStringKind::kMedium;
  rep_.medium.size = string.size();
  if (arena != nullptr) {
    auto* data = google::protobuf::Arena::Create<std::string>(arena, std::move(string));
    rep_.medium.data = data->data();
    rep_.medium.owner =
        reinterpret_cast<uintptr_t>(arena) | kMetadataOwnerArenaBit;
  } else {
    auto pair = MakeReferenceCountedString(std::move(string));
    rep_.medium.data = pair.second.data();
    rep_.medium.owner = reinterpret_cast<uintptr_t>(pair.first) |
                        kMetadataOwnerReferenceCountBit;
  }
}

void ByteString::SetMedium(absl::Nonnull<google::protobuf::Arena*> arena,
                           const absl::Cord& cord) {
  ABSL_DCHECK_GT(cord.size(), kSmallByteStringCapacity);
  rep_.header.kind = ByteStringKind::kMedium;
  rep_.medium.size = cord.size();
  char* data = static_cast<char*>(
      arena->AllocateAligned(rep_.medium.size, alignof(char)));
  (CopyCordToArray)(cord, data);
  rep_.medium.data = data;
  rep_.medium.owner =
      reinterpret_cast<uintptr_t>(arena) | kMetadataOwnerArenaBit;
}

void ByteString::SetMedium(absl::string_view string, uintptr_t owner) {
  ABSL_DCHECK_GT(string.size(), kSmallByteStringCapacity);
  ABSL_DCHECK_NE(owner, 0);
  rep_.header.kind = ByteStringKind::kMedium;
  rep_.medium.size = string.size();
  rep_.medium.data = string.data();
  rep_.medium.owner = owner;
}

void ByteString::SetLarge(const absl::Cord& cord) {
  ABSL_DCHECK_GT(cord.size(), kSmallByteStringCapacity);
  rep_.header.kind = ByteStringKind::kLarge;
  ::new (static_cast<void*>(&rep_.large.data[0])) absl::Cord(cord);
}

void ByteString::SetLarge(absl::Cord&& cord) {
  ABSL_DCHECK_GT(cord.size(), kSmallByteStringCapacity);
  rep_.header.kind = ByteStringKind::kLarge;
  ::new (static_cast<void*>(&rep_.large.data[0])) absl::Cord(std::move(cord));
}

absl::string_view LegacyByteString(const ByteString& string,
                                   absl::Nonnull<google::protobuf::Arena*> arena) {
  ABSL_DCHECK(arena != nullptr);
  if (string.empty()) {
    return absl::string_view();
  }
  const ByteStringKind kind = string.GetKind();
  if (kind == ByteStringKind::kMedium && string.GetMediumArena() == arena) {
    absl::Nullable<google::protobuf::Arena*> other_arena = string.GetMediumArena();
    if (other_arena == arena || other_arena == nullptr) {
      // Legacy values do not preserve arena. For speed, we assume the arena is
      // compatible.
      return string.GetMedium();
    }
  }
  absl::Nonnull<std::string*> result =
      google::protobuf::Arena::Create<std::string>(arena);
  switch (kind) {
    case ByteStringKind::kSmall:
      result->assign(string.GetSmall());
      break;
    case ByteStringKind::kMedium:
      result->assign(string.GetMedium());
      break;
    case ByteStringKind::kLarge:
      absl::CopyCordToString(string.GetLarge(), result);
      break;
  }
  return absl::string_view(*result);
}

ByteStringView::ByteStringView(const ByteString& other) {
  switch (other.GetKind()) {
    case ByteStringKind::kSmall: {
      auto* other_arena = other.GetSmallArena();
      const auto string = other.GetSmall();
      rep_.header.kind = ByteStringViewKind::kString;
      rep_.string.size = string.size();
      rep_.string.data = string.data();
      if (other_arena != nullptr) {
        rep_.string.owner =
            reinterpret_cast<uintptr_t>(other_arena) | kMetadataOwnerArenaBit;
      } else {
        rep_.string.owner = 0;
      }
    } break;
    case ByteStringKind::kMedium: {
      const auto string = other.GetMedium();
      rep_.header.kind = ByteStringViewKind::kString;
      rep_.string.size = string.size();
      rep_.string.data = string.data();
      rep_.string.owner = other.GetMediumOwner();
    } break;
    case ByteStringKind::kLarge: {
      const auto& cord = other.GetLarge();
      rep_.header.kind = ByteStringViewKind::kCord;
      rep_.cord.size = cord.size();
      rep_.cord.data = &cord;
      rep_.cord.pos = 0;
    } break;
  }
}

bool ByteStringView::empty() const {
  switch (GetKind()) {
    case ByteStringViewKind::kString:
      return rep_.string.size == 0;
    case ByteStringViewKind::kCord:
      return rep_.cord.size == 0;
  }
}

size_t ByteStringView::size() const {
  switch (GetKind()) {
    case ByteStringViewKind::kString:
      return rep_.string.size;
    case ByteStringViewKind::kCord:
      return rep_.cord.size;
  }
}

absl::optional<absl::string_view> ByteStringView::TryFlat() const {
  switch (GetKind()) {
    case ByteStringViewKind::kString:
      return GetString();
    case ByteStringViewKind::kCord:
      if (auto flat = GetCord().TryFlat(); flat) {
        return flat->substr(rep_.cord.pos, rep_.cord.size);
      }
      return absl::nullopt;
  }
}

bool ByteStringView::Equals(ByteStringView rhs) const {
  switch (GetKind()) {
    case ByteStringViewKind::kString:
      switch (rhs.GetKind()) {
        case ByteStringViewKind::kString:
          return GetString() == rhs.GetString();
        case ByteStringViewKind::kCord:
          return GetString() == rhs.GetSubcord();
      }
    case ByteStringViewKind::kCord:
      switch (rhs.GetKind()) {
        case ByteStringViewKind::kString:
          return GetSubcord() == rhs.GetString();
        case ByteStringViewKind::kCord:
          return GetSubcord() == rhs.GetSubcord();
      }
  }
}

int ByteStringView::Compare(ByteStringView rhs) const {
  switch (GetKind()) {
    case ByteStringViewKind::kString:
      switch (rhs.GetKind()) {
        case ByteStringViewKind::kString:
          return GetString().compare(rhs.GetString());
        case ByteStringViewKind::kCord:
          return -rhs.GetSubcord().Compare(GetString());
      }
    case ByteStringViewKind::kCord:
      switch (rhs.GetKind()) {
        case ByteStringViewKind::kString:
          return GetSubcord().Compare(rhs.GetString());
        case ByteStringViewKind::kCord:
          return GetSubcord().Compare(rhs.GetSubcord());
      }
  }
}

bool ByteStringView::StartsWith(ByteStringView rhs) const {
  switch (GetKind()) {
    case ByteStringViewKind::kString:
      switch (rhs.GetKind()) {
        case ByteStringViewKind::kString:
          return absl::StartsWith(GetString(), rhs.GetString());
        case ByteStringViewKind::kCord: {
          const auto string = GetString();
          const auto& cord = rhs.GetSubcord();
          const auto cord_size = cord.size();
          return string.size() >= cord_size &&
                 string.substr(0, cord_size) == cord;
        }
      }
    case ByteStringViewKind::kCord:
      switch (rhs.GetKind()) {
        case ByteStringViewKind::kString:
          return GetSubcord().StartsWith(rhs.GetString());
        case ByteStringViewKind::kCord:
          return GetSubcord().StartsWith(rhs.GetSubcord());
      }
  }
}

bool ByteStringView::EndsWith(ByteStringView rhs) const {
  switch (GetKind()) {
    case ByteStringViewKind::kString:
      switch (rhs.GetKind()) {
        case ByteStringViewKind::kString:
          return absl::EndsWith(GetString(), rhs.GetString());
        case ByteStringViewKind::kCord: {
          const auto string = GetString();
          const auto& cord = rhs.GetSubcord();
          const auto string_size = string.size();
          const auto cord_size = cord.size();
          return string_size >= cord_size &&
                 string.substr(string_size - cord_size) == cord;
        }
      }
    case ByteStringViewKind::kCord:
      switch (rhs.GetKind()) {
        case ByteStringViewKind::kString:
          return GetSubcord().EndsWith(rhs.GetString());
        case ByteStringViewKind::kCord:
          return GetSubcord().EndsWith(rhs.GetSubcord());
      }
  }
}

void ByteStringView::RemovePrefix(size_t n) {
  ABSL_DCHECK_LE(n, size());
  switch (GetKind()) {
    case ByteStringViewKind::kString:
      rep_.string.data += n;
      break;
    case ByteStringViewKind::kCord:
      rep_.cord.pos += n;
      break;
  }
  rep_.header.size -= n;
}

void ByteStringView::RemoveSuffix(size_t n) {
  ABSL_DCHECK_LE(n, size());
  rep_.header.size -= n;
}

std::string ByteStringView::ToString() const {
  switch (GetKind()) {
    case ByteStringViewKind::kString:
      return std::string(GetString());
    case ByteStringViewKind::kCord:
      return static_cast<std::string>(GetSubcord());
  }
}

void ByteStringView::CopyToString(absl::Nonnull<std::string*> out) const {
  switch (GetKind()) {
    case ByteStringViewKind::kString:
      out->assign(GetString());
      break;
    case ByteStringViewKind::kCord:
      absl::CopyCordToString(GetSubcord(), out);
      break;
  }
}

void ByteStringView::AppendToString(absl::Nonnull<std::string*> out) const {
  switch (GetKind()) {
    case ByteStringViewKind::kString:
      out->append(GetString());
      break;
    case ByteStringViewKind::kCord:
      absl::AppendCordToString(GetSubcord(), out);
      break;
  }
}

absl::Cord ByteStringView::ToCord() const {
  switch (GetKind()) {
    case ByteStringViewKind::kString: {
      const auto* refcount = GetStringReferenceCount();
      if (refcount != nullptr) {
        StrongRef(*refcount);
        return absl::MakeCordFromExternal(GetString(),
                                          ReferenceCountReleaser{refcount});
      }
      return absl::Cord(GetString());
    }
    case ByteStringViewKind::kCord:
      return GetSubcord();
  }
}

void ByteStringView::CopyToCord(absl::Nonnull<absl::Cord*> out) const {
  switch (GetKind()) {
    case ByteStringViewKind::kString: {
      const auto* refcount = GetStringReferenceCount();
      if (refcount != nullptr) {
        StrongRef(*refcount);
        *out = absl::MakeCordFromExternal(GetString(),
                                          ReferenceCountReleaser{refcount});
      } else {
        *out = absl::Cord(GetString());
      }
    } break;
    case ByteStringViewKind::kCord:
      *out = GetSubcord();
      break;
  }
}

void ByteStringView::AppendToCord(absl::Nonnull<absl::Cord*> out) const {
  switch (GetKind()) {
    case ByteStringViewKind::kString: {
      const auto* refcount = GetStringReferenceCount();
      if (refcount != nullptr) {
        StrongRef(*refcount);
        out->Append(absl::MakeCordFromExternal(
            GetString(), ReferenceCountReleaser{refcount}));
      } else {
        out->Append(GetString());
      }
    } break;
    case ByteStringViewKind::kCord:
      out->Append(GetSubcord());
      break;
  }
}

absl::Nullable<google::protobuf::Arena*> ByteStringView::GetArena() const {
  switch (GetKind()) {
    case ByteStringViewKind::kString:
      return GetStringArena();
    case ByteStringViewKind::kCord:
      return nullptr;
  }
}

void ByteStringView::HashValue(absl::HashState state) const {
  switch (GetKind()) {
    case ByteStringViewKind::kString:
      absl::HashState::combine(std::move(state), GetString());
      break;
    case ByteStringViewKind::kCord:
      absl::HashState::combine(std::move(state), GetSubcord());
      break;
  }
}

}  // namespace cel::common_internal
