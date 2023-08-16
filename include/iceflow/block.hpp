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
#include "opencv2/opencv.hpp" // TODO: Remove this dependency

#include "content-type-value.hpp"
#include "logger.hpp"
#include "typed-data.hpp"

namespace iceflow {

class Block {
public:
  Block() {}
  Block(ndn::Block block) { m_data = block; }

  Block(ndn::Block block, uint32_t type) {
    m_data = ndn::encoding::makeBinaryBlock(type, block.value_begin(),
                                            block.value_end());
  }

  void pushJson(JsonData inputJson) {
    std::string jsonString = inputJson.getJson().dump();
    NDN_LOG_INFO("IceFlow Block: " << jsonString);
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
      NDN_LOG_INFO("pullJson Manifest data");
      convertedJson = nlohmann::json::parse(resultSubElements[0].value_begin(),
                                            resultSubElements[0].value_end());
    } else {
      NDN_LOG_INFO("pullJson Main data");
      convertedJson =
          nlohmann::json::parse(m_data.value_begin(), m_data.value_end());
    }
    outputJson.setJson(convertedJson);
    return outputJson;
  }

  void pushFrame(cv::Mat frame) {
    std::vector<uchar> buffer;
    cv::imencode(".jpeg", frame, buffer);
    pushSegmentManifestBlock(buffer);
  }

  void pushFrameCompress(cv::Mat frame) {
    std::vector<uchar> buffer;
    std::vector<int> param(2);
    param[0] = cv::IMWRITE_JPEG_QUALITY;
    param[1] = 10; // default(95) 0-100
    cv::imencode(".jpeg", frame, buffer, param);
    pushSegmentManifestBlock(buffer);
  }

  cv::Mat pullFrame() {
    std::vector<ndn::Block> resultSubElements = m_data.elements();

    // test type here instead of the first element
    if (resultSubElements.size() > 0) {
      NDN_LOG_INFO("pullFrame Manifest data");
      for (int i = 0; i < resultSubElements.size(); i++) {
        if (resultSubElements[i].type() == ContentTypeValue::SegmentManifest) {
          std::vector<uint8_t> frameBuffer(resultSubElements[i].value_begin(),
                                           resultSubElements[i].value_end());
          return imdecode(cv::Mat(frameBuffer), 1);
        }
      }
    } else {
      NDN_LOG_INFO("pullFrame Main data");
      std::vector<uint8_t> frameBuffer(m_data.value_begin(),
                                       m_data.value_end());
      return imdecode(cv::Mat(frameBuffer), 1);
    }
  }

  void pushSegmentManifestBlock(std::vector<uchar> &buffer) {
    std::vector<uint8_t> &frameBuffer =
        reinterpret_cast<std::vector<uint8_t> &>(buffer);
    ndn::Block frameContent = ndn::encoding::makeBinaryBlock(
        ContentTypeValue::SegmentManifest, buffer);
    pushBlock(frameContent);
  }

  void pushBlock(ndn::Block block) { m_data.push_back(block); }

  void iceFlowBlockEncode() { m_data.encode(); }
  void iceFlowBlockParse() { m_data.parse(); }

  std::vector<ndn::Block> getSubElements() { return m_data.elements(); }

  void printInfo() {
    NDN_LOG_INFO("data type: " << m_data.type());
    auto test = getSubElements();
    if (test.size() > 0) {
      NDN_LOG_INFO("pullFrame Main data");
      NDN_LOG_INFO("test size: " << test.size());
      for (int i = 0; i < test.size(); i++) {
        NDN_LOG_INFO("Block type: " << test[i].type());
      }
    }
  }

private:
  ndn::Block m_data;
};

} // namespace iceflow

#endif // ICEFLOW_CORE_ICEFLOWBLOCK_HPP
