/*
 * Copyright 2024 The IceFlow Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "serde.hpp"

/**
 * Serialize nlohmann::json to CBOR format
 */
std::vector<uint8_t> Serde::serialize(const nlohmann::json &jsonData) {
  try {
    return nlohmann::json::to_cbor(jsonData);
  } catch (const std::exception &e) {
    throw std::runtime_error("Error during CBOR serialization: " +
                             std::string(e.what()));
  }
}

/**
 * Deserialize nlohmann::json to CBOR format
 */
nlohmann::json Serde::deserialize(const std::vector<uint8_t> &cborData) {
  try {
    return nlohmann::json::from_cbor(cborData);
  } catch (const std::exception &e) {
    throw std::runtime_error("Error during CBOR deserialization: " +
                             std::string(e.what()));
  }
}
