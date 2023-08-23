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

namespace starkware {

template <typename FieldElementT>
FieldElementT JsonValue::AsFieldElement() const {
  AssertString();
  return FieldElementT::FromString(AsString());
}

template <typename FieldElementT>
std::vector<FieldElementT> JsonValue::AsFieldElementVector() const {
  return AsVector<FieldElementT>(
      [](const JsonValue& value) { return value.AsFieldElement<FieldElementT>(); });
}

template <typename T, typename Func>
std::vector<T> JsonValue::AsVector(const Func& func) const {
  AssertArray();
  std::vector<T> res;
  res.reserve(ArrayLength());
  for (size_t i = 0; i < ArrayLength(); i++) {
    res.push_back(func((*this)[i]));
  }
  return res;
}

}  // namespace starkware
