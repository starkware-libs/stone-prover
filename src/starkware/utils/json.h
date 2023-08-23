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

#ifndef STARKWARE_UTILS_JSON_H_
#define STARKWARE_UTILS_JSON_H_

#include <string>
#include <utility>
#include <vector>

#include "third_party/jsoncpp/json/json.h"

#include "starkware/algebra/polymorphic/field.h"
#include "starkware/error_handling/error_handling.h"

namespace starkware {

namespace json_builder {

namespace details {

class ValueReference;

}  // namespace details

}  // namespace json_builder

/*
  A thin wrapper around Json::Value that provides clear error messages.

  Usage example:
    auto root = JsonValue::FromFile("a.json");
    // Assuming a.json looks like: {'a': {'b': 5}}
    LOG(INFO) << root["a"]["b"].AsSizeT();
*/
class JsonValue {
 public:
  static JsonValue FromJsonCppValue(const Json::Value& value);

  static JsonValue FromFile(const std::string& filename);

  static JsonValue FromString(const std::string& json_content);

  static JsonValue EmptyArray();

  void Write(const std::string& filename) const;

  bool operator==(const JsonValue& other) const;

  bool operator!=(const JsonValue& other) const;

  JsonValue operator[](const std::string& name) const;

  JsonValue operator[](size_t idx) const;

  bool HasValue() const;

  bool AsBool() const;

  uint64_t AsUint64() const;

  /*
    Returns the value as uint64_t assuming it is given in a string.
  */
  uint64_t AsUint64FromString() const;

  size_t AsSizeT() const;

  size_t ArrayLength() const;

  std::string AsString() const;

  Field AsField() const;

  template <typename FieldElementT>
  FieldElementT AsFieldElement() const;

  std::vector<bool> AsBoolVector() const;

  std::vector<size_t> AsSizeTVector() const;

  template <typename FieldElementT>
  std::vector<FieldElementT> AsFieldElementVector() const;

  std::vector<std::string> Keys() const;

 private:
  JsonValue(Json::Value value, std::string path)
      : value_(std::move(value)), path_(std::move(path)) {}

  /*
    Fails if the current value is not an object.
  */
  void AssertObject() const;

  /*
    Fails if the current value is not an array.
  */
  void AssertArray() const;

  /*
    Fails if the current value is not a boolean.
  */
  void AssertBool() const;

  /*
    Fails if the current value is not an integer.
  */
  void AssertInt() const;

  /*
    Fails if the current value is not an unsigned integer.
  */
  void AssertUint64() const;

  /*
    Fails if the current value is not a string.
  */
  void AssertString() const;

  /*
    Helper function for the As*Vector() functions.
  */
  template <typename T, typename Func>
  std::vector<T> AsVector(const Func& func) const;

  Json::Value value_;
  /*
    The path from the beginning of the json file to this value.
    For example root["a"]["b"].path_ will be "/a/b/".
    This value is used for error messages.
  */
  const std::string path_;
  friend class json_builder::details::ValueReference;
};

}  // namespace starkware

#include "starkware/utils/json.inl"

#endif  // STARKWARE_UTILS_JSON_H_
