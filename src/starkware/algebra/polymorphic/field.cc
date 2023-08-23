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

#include "starkware/algebra/polymorphic/field.h"

#include <string>
#include <vector>

namespace starkware {

FieldElement Field::Zero() const { return wrapper_->Zero(); }

FieldElement Field::One() const { return wrapper_->One(); }

FieldElement Field::Generator() const { return wrapper_->Generator(); }

FieldElement Field::RandomElement(PrngBase* prng) const { return wrapper_->RandomElement(prng); }

FieldElement Field::FromBytes(gsl::span<const std::byte> bytes, bool use_big_endian) const {
  return wrapper_->FromBytes(bytes, use_big_endian);
}

FieldElement Field::FromString(const std::string& s) const { return wrapper_->FromString(s); }

size_t Field::ElementSizeInBytes() const { return wrapper_->ElementSizeInBytes(); }

bool Field::operator==(const Field& other) const { return wrapper_->Equals(other); }

bool Field::operator!=(const Field& other) const { return !(*this == other); }

}  // namespace starkware
