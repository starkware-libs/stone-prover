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

#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/polymorphic/field.h"
#include "starkware/error_handling/error_handling.h"

namespace starkware {

class FieldElement::WrapperBase {
 public:
  virtual ~WrapperBase() = default;
  virtual std::unique_ptr<WrapperBase> Clone() const = 0;
  virtual Field GetField() const = 0;
  virtual FieldElement operator+(const FieldElement& other) const = 0;
  virtual FieldElement operator-(const FieldElement& other) const = 0;
  virtual FieldElement operator-() const = 0;
  virtual FieldElement operator*(const FieldElement& other) const = 0;
  virtual FieldElement operator/(const FieldElement& other) const = 0;
  virtual FieldElement Inverse() const = 0;
  virtual FieldElement Pow(uint64_t exp) const = 0;
  virtual void ToBytes(gsl::span<std::byte> span_out, bool use_big_endian) const = 0;
  virtual bool Equals(const FieldElement& other) const = 0;
  virtual size_t SizeInBytes() const = 0;
  virtual std::string ToString() const = 0;
};

template <typename T>
class FieldElement::Wrapper : public WrapperBase {
 public:
  explicit Wrapper(const T& value) : value_(value) {}

  const T& Value() const { return value_; }

  std::unique_ptr<WrapperBase> Clone() const override {
    return std::make_unique<Wrapper<T>>(value_);
  }

  Field GetField() const override { return Field::Create<T>(); }

  FieldElement operator+(const FieldElement& other) const override {
    return FieldElement(value_ + other.As<T>());
  }

  FieldElement operator-(const FieldElement& other) const override {
    return FieldElement(value_ - other.As<T>());
  }

  FieldElement operator-() const override { return FieldElement(-value_); }

  FieldElement operator*(const FieldElement& other) const override {
    return FieldElement(value_ * other.As<T>());
  }

  FieldElement operator/(const FieldElement& other) const override {
    return FieldElement(value_ / other.As<T>());
  }

  FieldElement Inverse() const override { return FieldElement(value_.Inverse()); }

  FieldElement Pow(uint64_t exp) const override {
    return FieldElement(::starkware::Pow(value_, exp));
  }

  void ToBytes(gsl::span<std::byte> span_out, bool use_big_endian) const override {
    value_.ToBytes(span_out, use_big_endian);
  }

  bool Equals(const FieldElement& other) const override { return value_ == other.As<T>(); }

  size_t SizeInBytes() const override { return value_.SizeInBytes(); }

  std::string ToString() const override { return value_.ToString(); }

 private:
  const T value_;
};

template <typename T>
FieldElement::FieldElement(const T& t) : wrapper_(std::make_unique<Wrapper<T>>(t)) {}

template <typename T>
const T& FieldElement::As() const {
  auto* ptr = dynamic_cast<const Wrapper<T>*>(wrapper_.get());
  ASSERT_RELEASE(ptr != nullptr, "The underlying type of FieldElement is wrong");

  return ptr->Value();
}

}  // namespace starkware
