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

#ifndef STARKWARE_UTILS_JSON_BUILDER_H_
#define STARKWARE_UTILS_JSON_BUILDER_H_

#include <string>

#include "third_party/jsoncpp/json/json.h"

#include "starkware/algebra/field_element_base.h"
#include "starkware/utils/json.h"

namespace starkware {

namespace json_builder {

namespace details {

class ValueReference {
 public:
  explicit ValueReference(Json::Value* value) : value_(value) {}

  ValueReference operator[](const size_t& idx) {
    return ValueReference(&((*value_)[static_cast<int>(idx)]));
  }
  ValueReference operator[](const std::string& name) { return ValueReference(&((*value_)[name])); }

  /*
    Assigns value to the current json pointer.
  */
  template <typename T>
  ValueReference& operator=(const T& value) {
    *value_ = ToJson(value);
    return *this;
  }

  /*
    Appends a value to the current json object.
    Returns a reference to this, to allow concatenation of Append() calls to create an array.
  */
  template <typename T>
  ValueReference& Append(const T& value) {
    value_->append(ToJson(value));
    return *this;
  }

  JsonValue Value() { return JsonValue::FromJsonCppValue(*value_); }

 private:
  /*
    Constructs Json::Value from T, if Json::Value(T) is defined.
  */
  template <typename T>
  static std::enable_if_t<std::is_constructible_v<Json::Value, T>, Json::Value> ToJson(
      const T& value) {
    return Json::Value(value);  // NOLINT: clang-tidy complains about implicitly decaying an array
                                // into a pointer when using builder["a"] = "b";
  }

  /*
    Constructs Json::Value from FieldElementT.
  */
  template <typename FieldElementT>
  static std::enable_if_t<kIsFieldElement<FieldElementT>, Json::Value> ToJson(
      const FieldElementT& value) {
    return Json::Value(value.ToString());
  }

  /*
    Constructs Json::Value from JsonValue.
  */
  static Json::Value ToJson(const JsonValue& value) { return value.value_; }

  Json::Value* value_;
};

}  // namespace details

}  // namespace json_builder

/*
  Constructs a Json object.

  Example: To construct {'key': 'value', 'array': [1, 2]}:
    JsonBuilder builder;
    builder["key"] = "value";
    builder["array"].Append(1).Append(2);
    JsonValue json = builder.Build();
*/
class JsonBuilder {
 public:
  using ValueReference = json_builder::details::ValueReference;

  static JsonBuilder FromJsonValue(const JsonValue& value) {
    JsonBuilder builder;
    builder.Root() = value;
    return builder;
  }

  ValueReference operator[](const std::string& name) { return Root()[name]; }

  JsonValue Build() const { return JsonValue::FromJsonCppValue(root_); }

 private:
  ValueReference Root() { return ValueReference(&root_); }

  Json::Value root_;
};

}  // namespace starkware

#endif  // STARKWARE_UTILS_JSON_BUILDER_H_
