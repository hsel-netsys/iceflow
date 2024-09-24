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

#ifndef SERDE_HPP
#define SERDE_HPP

#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <vector>

class Serde {
public:
  /**
   * Serialize nlohmann::json to CBOR format
   */
  static std::vector<uint8_t> serialize(const nlohmann::json &jsonData);

  /**
   * Deserialize nlohmann::json to CBOR format
   */
  static nlohmann::json deserialize(const std::vector<uint8_t> &cborData);
};

#endif // SERDE_HPP
