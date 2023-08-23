// Copyright 2023 StarkWare Industries Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License").
// You may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.starkware.co/open-source-license/
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions
// and limitations under the License.

#include "starkware/utils/maybe_owned_ptr.h"

#include "gtest/gtest.h"

#include "starkware/error_handling/error_handling.h"

namespace starkware {
namespace {

/*
  DummyObj counts the number of living instances to make sure it is constructed and destructed
  properly.
*/
class DummyObj {
 public:
  DummyObj() {
    n_instances++;
    n_total_instances_created++;
  }
  virtual ~DummyObj() { n_instances--; }
  DummyObj(const DummyObj&) = delete;
  DummyObj& operator=(const DummyObj&) = delete;
  DummyObj& operator=(const DummyObj&&) = delete;

  DummyObj(DummyObj&& other) noexcept : DummyObj() {
    ASSERT_RELEASE(other.is_alive, "Expected move to happen from an instance with is_alive=true.");
    other.is_alive = false;
    n_moves++;
  }

  DummyObj* GetAddress() { return this; }

  /*
    This value is set to false when the object is moved to another object.
  */
  bool is_alive = true;
  static int n_instances;
  static int n_total_instances_created;
  static int n_moves;
};

class DummySubclass : public DummyObj {};

int DummyObj::n_instances = 0;
int DummyObj::n_total_instances_created = 0;
int DummyObj::n_moves = 0;

class MaybeOwnedPtrTest : public testing::Test {
 public:
  MaybeOwnedPtrTest() {
    // Make sure n_instances and n_total_instances_created are zero before each test.
    DummyObj::n_instances = 0;
    DummyObj::n_total_instances_created = 0;
    DummyObj::n_moves = 0;
  }
};

TEST_F(MaybeOwnedPtrTest, BasicFunctionality) {
  DummyObj obj;
  MaybeOwnedPtr<DummyObj> maybe_owned_ptr = UseOwned(&obj);
  EXPECT_EQ(&obj, maybe_owned_ptr.get());
  EXPECT_EQ(&obj, &(*maybe_owned_ptr));
  EXPECT_EQ(&obj, maybe_owned_ptr->GetAddress());

  // Test that the number of constructions & destructions is correct.
  EXPECT_EQ(1, DummyObj::n_instances);
  EXPECT_EQ(1, DummyObj::n_total_instances_created);
  EXPECT_EQ(0, DummyObj::n_moves);
}

TEST_F(MaybeOwnedPtrTest, NullptrConstructible) {
  {
    MaybeOwnedPtr<DummyObj> maybe_owned_ptr1 = nullptr;
    MaybeOwnedPtr<DummyObj> maybe_owned_ptr2;
    EXPECT_EQ(0, DummyObj::n_instances);
  }
  EXPECT_EQ(0, DummyObj::n_instances);
  EXPECT_EQ(0, DummyObj::n_total_instances_created);
  EXPECT_EQ(0, DummyObj::n_moves);
}

TEST_F(MaybeOwnedPtrTest, HasValue) {
  MaybeOwnedPtr<DummyObj> maybe_owned_ptr;
  EXPECT_FALSE(maybe_owned_ptr.HasValue());
  auto obj_ptr = std::make_unique<DummyObj>();
  maybe_owned_ptr = UseOwned(obj_ptr);
  EXPECT_TRUE(maybe_owned_ptr.HasValue());
  maybe_owned_ptr.reset();
  EXPECT_FALSE(maybe_owned_ptr.HasValue());
  maybe_owned_ptr = TakeOwnershipFrom(std::move(obj_ptr));
  EXPECT_TRUE(maybe_owned_ptr.HasValue());
}

TEST_F(MaybeOwnedPtrTest, UseOwned) {
  DummyObj obj;
  EXPECT_EQ(1, DummyObj::n_instances);
  {
    MaybeOwnedPtr<DummyObj> maybe_owned_ptr = UseOwned(&obj);
    EXPECT_EQ(1, DummyObj::n_instances);
    // Exiting the scope will free maybe_owned_ptr, but the inner object will still be alive.
  }
  EXPECT_EQ(1, DummyObj::n_instances);
  EXPECT_EQ(1, DummyObj::n_total_instances_created);
  EXPECT_EQ(0, DummyObj::n_moves);
}

TEST_F(MaybeOwnedPtrTest, UseOwnedMaybeOwned) {
  DummyObj obj;
  EXPECT_EQ(1, DummyObj::n_instances);
  MaybeOwnedPtr<DummyObj> maybe_owned1 = UseOwned(&obj);
  {
    MaybeOwnedPtr<DummyObj> maybe_owned2 = UseOwned(maybe_owned1);
    EXPECT_EQ(1, DummyObj::n_instances);
    // Exiting the scope will free maybe_owned2, but the inner object will still be alive.
  }
  EXPECT_EQ(1, DummyObj::n_instances);
  EXPECT_EQ(1, DummyObj::n_total_instances_created);
  EXPECT_EQ(0, DummyObj::n_moves);
}

TEST_F(MaybeOwnedPtrTest, UseOwnedUniquePtr) {
  auto obj_ptr = std::make_unique<DummyObj>();
  EXPECT_EQ(1, DummyObj::n_instances);
  {
    MaybeOwnedPtr<DummyObj> maybe_owned = UseOwned(obj_ptr);
    EXPECT_EQ(1, DummyObj::n_instances);
    // Exiting the scope will free maybe_owned, but the inner object will still be alive.
  }
  EXPECT_EQ(1, DummyObj::n_instances);
  EXPECT_EQ(1, DummyObj::n_total_instances_created);
  EXPECT_EQ(0, DummyObj::n_moves);
}

TEST_F(MaybeOwnedPtrTest, TakeOwnershipFrom) {
  auto obj_ptr = std::make_unique<DummyObj>();
  EXPECT_EQ(1, DummyObj::n_instances);
  {
    EXPECT_NE(nullptr, obj_ptr);
    MaybeOwnedPtr<DummyObj> maybe_owned_ptr = TakeOwnershipFrom(std::move(obj_ptr));
    // After taking ownership, obj_ptr becomes nullptr.
    EXPECT_EQ(1, DummyObj::n_instances);
    EXPECT_EQ(nullptr, obj_ptr);
    // Exiting the scope will free maybe_owned_ptr, including the inner object.
  }
  EXPECT_EQ(0, DummyObj::n_instances);
  EXPECT_EQ(1, DummyObj::n_total_instances_created);
  EXPECT_EQ(0, DummyObj::n_moves);
}

TEST_F(MaybeOwnedPtrTest, UseMovedValue) {
  DummyObj obj;
  EXPECT_EQ(1, DummyObj::n_instances);
  {
    EXPECT_TRUE(obj.is_alive);
    MaybeOwnedPtr<DummyObj> maybe_owned_ptr = UseMovedValue(std::move(obj));
    EXPECT_FALSE(obj.is_alive);  // NOLINT: use object after move.
    EXPECT_EQ(2, DummyObj::n_instances);
    // Exiting the scope will free maybe_owned_ptr, including the inner object.
  }
  EXPECT_EQ(1, DummyObj::n_instances);
  EXPECT_EQ(2, DummyObj::n_total_instances_created);
  EXPECT_EQ(1, DummyObj::n_moves);
}

TEST_F(MaybeOwnedPtrTest, ConstructFromSubclass) {
  auto obj_ptr = std::make_unique<DummySubclass>();
  EXPECT_EQ(1, DummyObj::n_instances);
  {
    EXPECT_NE(nullptr, obj_ptr);
    MaybeOwnedPtr<DummyObj> maybe_owned_ptr = TakeOwnershipFrom(std::move(obj_ptr));
    EXPECT_EQ(nullptr, obj_ptr);
    EXPECT_EQ(1, DummyObj::n_instances);
  }
  EXPECT_EQ(0, DummyObj::n_instances);
  EXPECT_EQ(1, DummyObj::n_total_instances_created);
  EXPECT_EQ(0, DummyObj::n_moves);
}

TEST_F(MaybeOwnedPtrTest, MoveConstructorNotOwned) {
  DummyObj obj;
  EXPECT_EQ(1, DummyObj::n_instances);
  {
    MaybeOwnedPtr<DummyObj> maybe_owned_ptr1 = UseOwned(&obj);
    MaybeOwnedPtr<DummyObj> maybe_owned_ptr2 = std::move(maybe_owned_ptr1);
    MaybeOwnedPtr<DummyObj> maybe_owned_ptr3 = std::move(maybe_owned_ptr2);
    EXPECT_EQ(nullptr, maybe_owned_ptr1.get());  // NOLINT: test value after move.
    EXPECT_EQ(nullptr, maybe_owned_ptr2.get());  // NOLINT: test value after move.
    EXPECT_EQ(&obj, maybe_owned_ptr3.get());
  }
  EXPECT_EQ(1, DummyObj::n_instances);
  EXPECT_EQ(1, DummyObj::n_total_instances_created);
  EXPECT_EQ(0, DummyObj::n_moves);
}

TEST_F(MaybeOwnedPtrTest, MoveConstructorOwned) {
  auto obj_ptr = std::make_unique<DummyObj>();
  EXPECT_EQ(1, DummyObj::n_instances);
  {
    EXPECT_NE(nullptr, obj_ptr);
    MaybeOwnedPtr<DummyObj> maybe_owned_ptr1 = TakeOwnershipFrom(std::move(obj_ptr));
    MaybeOwnedPtr<DummyObj> maybe_owned_ptr2 = std::move(maybe_owned_ptr1);
    MaybeOwnedPtr<DummyObj> maybe_owned_ptr3 = std::move(maybe_owned_ptr2);
    EXPECT_EQ(nullptr, maybe_owned_ptr1.get());  // NOLINT: test value after move.
    EXPECT_EQ(nullptr, maybe_owned_ptr2.get());  // NOLINT: test value after move.
    EXPECT_NE(nullptr, maybe_owned_ptr3.get());
  }
  EXPECT_EQ(0, DummyObj::n_instances);
  EXPECT_EQ(1, DummyObj::n_total_instances_created);
  EXPECT_EQ(0, DummyObj::n_moves);
}

TEST_F(MaybeOwnedPtrTest, PointerChanges) {
  auto obj_ptr = std::make_unique<DummyObj>();
  EXPECT_EQ(1, DummyObj::n_instances);
  {
    MaybeOwnedPtr<DummyObj> maybe_owned_ptr1 = nullptr;
    maybe_owned_ptr1 = TakeOwnershipFrom(std::move(obj_ptr));
    EXPECT_EQ(nullptr, obj_ptr);

    MaybeOwnedPtr<DummyObj> maybe_owned_ptr2 = TakeOwnershipFrom(std::make_unique<DummyObj>());
    EXPECT_EQ(2, DummyObj::n_instances);

    maybe_owned_ptr2 = std::move(maybe_owned_ptr1);
    EXPECT_EQ(1, DummyObj::n_instances);
    EXPECT_EQ(nullptr, maybe_owned_ptr1.get());  // NOLINT: test value after move.

    // The following line will free the inner object.
    maybe_owned_ptr2 = nullptr;
    EXPECT_EQ(nullptr, maybe_owned_ptr2.get());
    EXPECT_EQ(0, DummyObj::n_instances);
  }
  EXPECT_EQ(0, DummyObj::n_instances);
  EXPECT_EQ(2, DummyObj::n_total_instances_created);
  EXPECT_EQ(0, DummyObj::n_moves);
}

}  // namespace
}  // namespace starkware
