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

#include "starkware/algebra/polymorphic/field_element_span.h"

namespace starkware {

class FieldElementVector::WrapperBase {
 public:
  virtual ~WrapperBase() = default;
  virtual size_t Size() const = 0;
  virtual FieldElement operator[](size_t index) const = 0;
  FieldElement At(size_t index) const { return (*this)[index]; }
  virtual void Set(size_t index, const FieldElement& elt) = 0;
  virtual void PushBack(const FieldElement& elt) = 0;
  virtual void Reserve(size_t size) = 0;
  virtual Field GetField() const = 0;
  virtual bool operator==(const WrapperBase& other) const = 0;
  bool operator!=(const WrapperBase& other) const { return !(*this == other); }
  template <typename FieldElementT>
  std::vector<FieldElementT>& As();
  template <typename FieldElementT>
  const std::vector<FieldElementT>& As() const;
  virtual FieldElementSpan AsSpan() = 0;
  virtual ConstFieldElementSpan AsSpan() const = 0;
};

template <typename FieldElementT>
class FieldElementVector::Wrapper : public FieldElementVector::WrapperBase {
 public:
  explicit Wrapper(std::vector<FieldElementT> vec) : value_(std::move(vec)) {}

  size_t Size() const override { return value_.size(); }

  const std::vector<FieldElementT>& Value() const { return value_; }

  std::vector<FieldElementT>& Value() { return value_; }

  FieldElement operator[](size_t index) const override { return FieldElement(value_[index]); }

  void Set(size_t index, const FieldElement& elt) override {
    value_.at(index) = elt.As<FieldElementT>();
  }

  void PushBack(const FieldElement& elt) override { value_.push_back(elt.As<FieldElementT>()); }

  void Reserve(size_t size) override { value_.reserve(size); }

  Field GetField() const override { return Field::Create<FieldElementT>(); }

  bool operator==(const WrapperBase& other) const override {
    return value_ == other.As<FieldElementT>();
  }

  FieldElementSpan AsSpan() override { return FieldElementSpan(gsl::span<FieldElementT>(value_)); }

  ConstFieldElementSpan AsSpan() const override {
    return ConstFieldElementSpan(gsl::span<const FieldElementT>(value_));
  }

  static std::unique_ptr<WrapperBase> MakeUninitialized(size_t size) {
    return std::make_unique<Wrapper>(FieldElementT::UninitializedVector(size));
  }

  static std::unique_ptr<WrapperBase> Make(std::vector<FieldElementT>&& vec) {
    return std::make_unique<Wrapper>(std::move(vec));
  }

 private:
  std::vector<FieldElementT> value_;
};

template <typename FieldElementT>
std::vector<FieldElementT>& FieldElementVector::WrapperBase::As() {
  auto* ptr = dynamic_cast<Wrapper<FieldElementT>*>(this);
  ASSERT_RELEASE(ptr != nullptr, "The underlying type of FieldElementVector is wrong");
  return ptr->Value();
}

template <typename FieldElementT>
const std::vector<FieldElementT>& FieldElementVector::WrapperBase::As() const {
  auto* ptr = dynamic_cast<const Wrapper<FieldElementT>*>(this);
  ASSERT_RELEASE(ptr != nullptr, "The underlying type of FieldElementVector is wrong");
  return ptr->Value();
}

template <typename FieldElementT>
std::vector<FieldElementT>& FieldElementVector::As() {
  return wrapper_->As<FieldElementT>();
}

template <typename FieldElementT>
const std::vector<FieldElementT>& FieldElementVector::As() const {
  return static_cast<const WrapperBase&>(*wrapper_).As<FieldElementT>();
}

template <typename FieldElementT>
FieldElementVector FieldElementVector::MakeUninitialized(const size_t size) {
  return FieldElementVector(Wrapper<FieldElementT>::MakeUninitialized(size));
}

template <typename FieldElementT>
FieldElementVector FieldElementVector::Make(std::vector<FieldElementT>&& vec) {
  return FieldElementVector(Wrapper<FieldElementT>::Make(std::move(vec)));
}

template <typename FieldElementT>
FieldElementVector FieldElementVector::CopyFrom(const std::vector<FieldElementT>& vec) {
  // Don't reuse span implementation as it is likely that vector copying is more optimized.
  return FieldElementVector::Make(std::vector<FieldElementT>(vec.begin(), vec.end()));
}

template <typename FieldElementT>
FieldElementVector FieldElementVector::CopyFrom(gsl::span<const FieldElementT> values) {
  return FieldElementVector::Make(std::vector<FieldElementT>(values.begin(), values.end()));
}

}  // namespace starkware
