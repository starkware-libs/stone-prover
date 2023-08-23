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

#include "starkware/utils/json.h"

#include <fstream>
#include <optional>

#include "starkware/algebra/utils/name_to_field.h"

namespace starkware {

JsonValue JsonValue::FromJsonCppValue(const Json::Value& value) { return JsonValue(value, "/"); }

JsonValue JsonValue::FromFile(const std::string& filename) {
  std::ifstream file(filename);

  ASSERT_RELEASE(static_cast<bool>(file), "Could not open \"" + filename + "\" for reading.");
  Json::Value root;
  try {
    file >> root;
  } catch (const Json::RuntimeError&) {
    THROW_STARKWARE_EXCEPTION("Failed to parse JSON file: \"" + filename + "\"");
  }
  return JsonValue(root, "/");
}

JsonValue JsonValue::FromString(const std::string& json_content) {
  Json::Value root;
  std::istringstream s(json_content);
  s >> root;
  return JsonValue(root, "/");
}

JsonValue JsonValue::EmptyArray() { return JsonValue(Json::arrayValue, "/"); }

void JsonValue::Write(const std::string& filename) const {
  std::ofstream file(filename);
  ASSERT_RELEASE(static_cast<bool>(file), "Could not open \"" + filename + "\" for writing.");
  file << value_;
}

bool JsonValue::operator==(const JsonValue& other) const { return value_ == other.value_; }

bool JsonValue::operator!=(const JsonValue& other) const { return !(*this == other); }

JsonValue JsonValue::operator[](const std::string& name) const {
  AssertObject();
  return JsonValue(value_[name], path_ + name + "/");
}

JsonValue JsonValue::operator[](size_t idx) const {
  AssertArray();
  auto idx2 = static_cast<Json::ArrayIndex>(idx);
  ASSERT_RELEASE(
      idx2 < ArrayLength(), "Index " + std::to_string(idx) + " is out of range in " + path_ + ".");
  return JsonValue(value_[idx2], path_ + std::to_string(idx) + "/");
}

bool JsonValue::HasValue() const { return !value_.isNull(); }

bool JsonValue::AsBool() const {
  AssertBool();
  return value_.asBool();
}

uint64_t JsonValue::AsUint64() const {
  AssertUint64();
  return value_.asUInt64();
}

uint64_t JsonValue::AsUint64FromString() const {
  const std::string str = AsString();
  return StrToUint64(str);
}

size_t JsonValue::AsSizeT() const {
  AssertInt();
  return value_.asUInt();
}

size_t JsonValue::ArrayLength() const {
  AssertArray();
  return value_.size();
}

std::string JsonValue::AsString() const {
  AssertString();
  return value_.asString();
}

std::vector<bool> JsonValue::AsBoolVector() const {
  return AsVector<bool>([](const JsonValue& v) { return v.AsBool(); });
}

std::vector<size_t> JsonValue::AsSizeTVector() const {
  return AsVector<size_t>([](const JsonValue& v) { return v.AsSizeT(); });
}

Field JsonValue::AsField() const {
  std::string field_name = AsString();
  std::optional<starkware::Field> optional_field = starkware::NameToField(field_name);
  ASSERT_RELEASE(optional_field, "Undefined field '" + field_name + "'.");
  return *optional_field;
}

std::vector<std::string> JsonValue::Keys() const {
  AssertObject();
  return value_.getMemberNames();
}

void JsonValue::AssertObject() const {
  ASSERT_RELEASE(!value_.isNull(), "Missing configuration object: " + path_);
  ASSERT_RELEASE(value_.isObject(), "Configuration at " + path_ + " is expected to be an object.");
}

void JsonValue::AssertArray() const {
  ASSERT_RELEASE(!value_.isNull(), "Missing configuration array: " + path_);
  ASSERT_RELEASE(value_.isArray(), "Configuration at " + path_ + " is expected to be an array.");
}

void JsonValue::AssertBool() const {
  ASSERT_RELEASE(!value_.isNull(), "Missing configuration value: " + path_);
  ASSERT_RELEASE(value_.isBool(), "Configuration at " + path_ + " is expected to be a boolean.");
}

void JsonValue::AssertInt() const {
  ASSERT_RELEASE(!value_.isNull(), "Missing configuration value: " + path_);
  ASSERT_RELEASE(
      value_.isIntegral(), "Configuration at " + path_ + " is expected to be an integer.");
}

void JsonValue::AssertUint64() const {
  ASSERT_RELEASE(!value_.isNull(), "Missing configuration value: " + path_);
  ASSERT_RELEASE(value_.isUInt64(), "Configuration at " + path_ + " is expected to be a uint64.");
}

void JsonValue::AssertString() const {
  ASSERT_RELEASE(!value_.isNull(), "Missing configuration value: " + path_);
  ASSERT_RELEASE(value_.isString(), "Configuration at " + path_ + " is expected to be a string.");
}

}  // namespace starkware
