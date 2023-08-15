/*
 * Copyright 2021 The IceFlow Authors.
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

#ifndef ICEFLOW_CORE_ANY_DATA_HPP
#define ICEFLOW_CORE_ANY_DATA_HPP

#include "nlohmann/json.hpp"

namespace iceflow {

// Abstract base class.
class TypedData {
public:
  virtual size_t getSize() = 0;
};

class JsonData : public TypedData {
public:
  void setJson(nlohmann::json jsnew) { m_json = jsnew; }

  nlohmann::json getJson() { return m_json; }
  size_t getSize() { return m_json.dump().size(); }

private:
  nlohmann::json m_json;
  size_t m_size;
};

} // namespace iceflow

#endif // ICEFLOW_CORE_ANY_DATA_HPP
