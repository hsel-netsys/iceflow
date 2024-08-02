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
  Block();
  explicit Block(ndn::Block block);

  Block(ndn::Block block, uint32_t type);

  void pushJson(JsonData inputJson);

  JsonData pullJson();

  void pushSegmentManifestBlock(std::vector<uint8_t> &buffer);

  void pushBlock(ndn::Block block);

  void iceFlowBlockEncode();
  void iceFlowBlockParse();

  std::vector<ndn::Block> getSubElements();

  ndn::Block getData();

private:
  ndn::Block m_data;
};

} // namespace iceflow

#endif // ICEFLOW_CORE_ICEFLOWBLOCK_HPP
