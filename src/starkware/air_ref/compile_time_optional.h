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
  Allows to mark members of a class as "hidden" to prevent accidental usage of their values in code.

  Usage example:

    template <int N>
    struct Foo {
        CompileTimeOptional<int, (N > 0)> x0 = 10;
        CompileTimeOptional<int, (N > 1)> x1 = 20;
        CompileTimeOptional<int, (N > 2)> x2 = 40;
    };

    ...

    Foo<2> foo;
    int val1 = foo.x0 + foo.x1; // This will compile.
    int val2 = foo.x0 + foo.x1 + foo.x2; // This will NOT compile (as x2 is hidden in the case N=2).
*/

#ifndef STARKWARE_AIR_COMPILE_TIME_OPTIONAL_H_
#define STARKWARE_AIR_COMPILE_TIME_OPTIONAL_H_

#include <type_traits>
#include <utility>

namespace starkware {

/*
  A type for class members which should not be available.
  Utility class for CompileTimeOptional.
*/
template <typename T>
class HiddenMember {
 public:
  HiddenMember(T value) : value_(std::move(value)) {}  // NOLINT: Implicit constructor.

  const T& ExtractValue() const { return value_; }

 private:
  T value_;
};

/*
  Used to hide a member of a class as a function of template arguments to prevent accidental usage
  of that member.
*/
template <typename T, bool Visible>
using CompileTimeOptional = std::conditional_t<Visible, T, HiddenMember<T>>;

/*
  Retrieves the value of a CompileTimeOptional instance, even if the member is hidden.
  Note that if the member is not hidden, it can be used as any other member and this function is not
  necessary.
*/
template <typename T>
const T& ExtractHiddenMemberValue(const HiddenMember<T>& value) {
  return value.ExtractValue();
}

template <typename T>
const T& ExtractHiddenMemberValue(const T& value) {
  return value;
}

}  // namespace starkware

#endif  // STARKWARE_AIR_COMPILE_TIME_OPTIONAL_H_
