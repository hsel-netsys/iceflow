/*
 * Copyright 2023 The IceFlow Authors.
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

#include "util.hpp"
#include "iceflow/content-type-value.hpp"

void pushFrame(iceflow::Block *block, cv::Mat frame) {
  std::vector<uchar> buffer;
  cv::imencode(".jpeg", frame, buffer);
  std::vector<uint8_t> &frameBuffer =
      reinterpret_cast<std::vector<uint8_t> &>(buffer);
  block->pushSegmentManifestBlock(frameBuffer);
}

void pushFrameCompress(iceflow::Block *block, cv::Mat frame) {
  std::vector<uchar> buffer;
  std::vector<int> param(2);
  param[0] = cv::IMWRITE_JPEG_QUALITY;
  param[1] = 10; // default(95) 0-100
  cv::imencode(".jpeg", frame, buffer, param);
  block->pushSegmentManifestBlock(buffer);
}

cv::Mat pullFrame(iceflow::Block block) {
  std::vector<ndn::Block> resultSubElements = block.getSubElements();

  // test type here instead of the first element
  if (resultSubElements.size() > 0) {
    NDN_LOG_INFO("pullFrame Manifest data");
    for (auto resultSubElement : resultSubElements) {
      if (resultSubElement.type() ==
          iceflow::ContentTypeValue::SegmentsInManifest) {
        std::vector<uint8_t> frameBuffer(resultSubElement.value_begin(),
                                         resultSubElement.value_end());
        //		return decode(frameBuffer);
        return imdecode(cv::Mat(frameBuffer), 1);
      }
    }
  }

  NDN_LOG_INFO("pullFrame Main data");
  auto data = block.getData();
  std::vector<uint8_t> frameBuffer(data.value_begin(), data.value_end());
  return imdecode(cv::Mat(frameBuffer), 1);
}
