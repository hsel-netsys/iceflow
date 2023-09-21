/*
 * Copyright 2022 The IceFlow Authors.
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

#ifndef ICEFLOW_CORE_ICEFLOWBLOCK_HPP
#define ICEFLOW_CORE_ICEFLOWBLOCK_HPP

#include "ndn-cxx/data.hpp"
#include "nlohmann/json.hpp"

#include "content-type-value.hpp"
#include "logger.hpp"
#include "typed-data.hpp"

namespace iceflow {

class Block {
public:
  Block() {}
  explicit Block(ndn::Block block) : m_data(block) {}

  Block(ndn::Block block, uint32_t type)
      : m_data(ndn::encoding::makeBinaryBlock(type, block.value_begin(),
                                              block.value_end())) {}

  void pushJson(JsonData inputJson) {
    std::string jsonString = inputJson.getJson().dump();
    NDN_LOG_DEBUG("IceFlow Block: " << jsonString);
    ndn::Block jsonContent = ndn::encoding::makeBinaryBlock(
        ContentTypeValue::Json, jsonString.begin(), jsonString.end());
    pushBlock(jsonContent);
  }

  JsonData pullJson() {
    std::vector<ndn::Block> resultSubElements = m_data.elements();
    JsonData outputJson;
    nlohmann::json convertedJson;
    // test type here instead of the first element -- fix it
    if (resultSubElements.size() > 0) { // fix this
      convertedJson = nlohmann::json::parse(resultSubElements[0].value_begin(),
                                            resultSubElements[0].value_end());
    } else {
      convertedJson =
          nlohmann::json::parse(m_data.value_begin(), m_data.value_end());
    }
    outputJson.setJson(convertedJson);
    return outputJson;
  }

  void pushSegmentManifestBlock(std::vector<uint8_t> &buffer) {
    ndn::Block frameContent = ndn::encoding::makeBinaryBlock(
        ContentTypeValue::SegmentManifest, buffer);
    pushBlock(frameContent);
  }

  void pushBlock(ndn::Block block) { m_data.push_back(block); }

  void iceFlowBlockEncode() { m_data.encode(); }
  void iceFlowBlockParse() { m_data.parse(); }

  std::vector<ndn::Block> getSubElements() { return m_data.elements(); }

  ndn::Block getData() { return m_data; }

private:
  ndn::Block m_data;
};

} // namespace iceflow

#endif // ICEFLOW_CORE_ICEFLOWBLOCK_HPP
