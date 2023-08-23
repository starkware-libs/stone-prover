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

/*
  A MaybeOwnedPtr is a smart pointer that can be in one of two states:
  1. Owned - The instance owns the pointer (it behaves like std::unique_ptr).
  2. Not owned - The instance is not owned and behaves like a raw pointer.

  The idea is that a class may want to hold a pointer to another class, but be agnostic to whether
  it should take ownership on this pointer or leave the ownership to be managed by the caller.

  As an example, consider the following class, A, which uses the services of another class, B:
    class A {
     public:
      A(B* b) : b_(b) {}

      UseB() { b_->Foo(); }

     private:
      B* b_;
    };

  Note that we could have replaced B* with std::unique_ptr<B>. Let's compare the two approaches:

  1. Using a raw pointer: A doesn't own b_. It allows using the same instance of B in multiple
     instances of A, which would be impossible otherwise.
  2. Using a unique_ptr<>: A owns b_. The lifetime of B is easier to handle. For example, a
     function, MakeA, whose signature is "A MakeA()" may create an instance of B and pass it to the
     newly created A.

  However, it should not be up to the programmer of A to decide whether A should take ownership of
  b_ or not. It should be decided by the code constructing A.

  This is solved by using a MaybeOwnedPtr, as follows:
  1. b_ is of type MaybeOwnedPtr<B>.
  2. A's constructor gets a MaybeOwnedPtr<B> and moves it to b_, just like a unique_ptr.
  3. A can be constructed in two forms:
     a. A(UseOwned(b_ptr)) where b_ptr is of type B* and it is owned by the caller.
     b. A(TakeOwnershipFrom(std::move(b_unique_ptr))) where b_unique_ptr is of type
        std::unique_ptr<B> and its ownership is transferred to A.
*/

#ifndef STARKWARE_UTILS_MAYBE_OWNED_PTR_H_
#define STARKWARE_UTILS_MAYBE_OWNED_PTR_H_

#include <memory>
#include <type_traits>
#include <utility>

namespace starkware {

// Forward declarations to allow having friend declarations in the class.

template <typename T>
class MaybeOwnedPtr;

template <typename T>
MaybeOwnedPtr<T> UseOwned(T* ptr);

template <typename T>
MaybeOwnedPtr<T> TakeOwnershipFrom(std::unique_ptr<T> ptr);

template <typename T>
class MaybeOwnedPtr {
 public:
  T& operator*() const { return *ptr_; }
  T* operator->() const { return ptr_; }
  T* get() const { return ptr_; }  // NOLINT: To be consistent with std::unique_ptr.

  MaybeOwnedPtr() = default;

  /*
    Constructor from nullptr.
  */
  MaybeOwnedPtr(std::nullptr_t) {}  // NOLINT: implicit cast.

  ~MaybeOwnedPtr() = default;
  MaybeOwnedPtr(const MaybeOwnedPtr& other) = delete;
  MaybeOwnedPtr& operator=(const MaybeOwnedPtr& other) = delete;

  MaybeOwnedPtr(MaybeOwnedPtr&& other) noexcept {
    // Use move assignment operator.
    *this = std::move(other);
  }

  MaybeOwnedPtr& operator=(MaybeOwnedPtr&& other) noexcept {
    ptr_ = other.ptr_;
    unique_ptr_ = std::move(other.unique_ptr_);

    other.reset();
    return *this;
  }

  /*
    Constructor from another MaybeOwnerPtr whose type may be a subclass of T.
  */
  template <typename U, typename std::enable_if<std::is_convertible<U*, T*>::value, int>::type = 0>
  MaybeOwnedPtr(MaybeOwnedPtr<U>&& other)  // NOLINT: implicit cast.
      : ptr_(other.ptr_), unique_ptr_(std::move(other.unique_ptr_)) {
    other.reset();
  }

  void reset() {  // NOLINT: To be consistent with std::unique_ptr.
    ptr_ = nullptr;
    unique_ptr_.reset();
  }

  bool HasValue() const { return ptr_ != nullptr; }

 private:
  explicit MaybeOwnedPtr(std::unique_ptr<T> ptr) : ptr_(ptr.get()), unique_ptr_(std::move(ptr)) {}
  explicit MaybeOwnedPtr(T* ptr) : ptr_(ptr), unique_ptr_() {}

  T* ptr_ = nullptr;
  std::unique_ptr<T> unique_ptr_ = nullptr;

  // Friend declarations.

  friend MaybeOwnedPtr<T> UseOwned<T>(T* ptr);
  friend MaybeOwnedPtr<T> TakeOwnershipFrom<T>(std::unique_ptr<T> ptr);

  template <typename U>
  friend class MaybeOwnedPtr;
};

template <typename T>
MaybeOwnedPtr<T> UseOwned(T* ptr) {
  return MaybeOwnedPtr<T>(ptr);
}

template <typename T>
MaybeOwnedPtr<T> UseOwned(const MaybeOwnedPtr<T>& other) {
  return UseOwned(other.get());
}

template <typename T>
MaybeOwnedPtr<T> UseOwned(const std::unique_ptr<T>& ptr) {
  return UseOwned(ptr.get());
}

template <typename T>
MaybeOwnedPtr<T> TakeOwnershipFrom(std::unique_ptr<T> ptr) {
  return MaybeOwnedPtr<T>(std::move(ptr));
}

/*
  Constructs a MaybeOwnedPtr<T> in an Owned state with the given value by calling T's move
  constructor.
*/
template <typename T, typename = typename std::enable_if<!std::is_lvalue_reference<T>::value>::type>
MaybeOwnedPtr<T> UseMovedValue(T&& value) {
  // Use std::forward instead of std::move to pass clang-tidy check. Both are fine in this case.
  return TakeOwnershipFrom(std::make_unique<T>(std::forward<T>(value)));
}

}  // namespace starkware

#endif  // STARKWARE_UTILS_MAYBE_OWNED_PTR_H_
