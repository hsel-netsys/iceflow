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

#ifndef ICEFLOW_CORE_ICEFLOW_DATA_HPP
#define ICEFLOW_CORE_ICEFLOW_DATA_HPP

#include <span>

#include "ndn-cxx/data.hpp"

#include "typed-data.hpp"

namespace iceflow {

class Data : public ndn::Data {
public:
  class Error : public ndn::Data::Error {
  public:
    using ndn::Data::Error::Error;
  };

  // constructors
  Data() {}

  explicit Data(const ndn::Block &wire) { wireDecode(wire); }

  // for manifests
  Data(const ndn::Name &name, const std::vector<ndn::Name> &namesInput,
       ContentTypeValue type)
      : ndn::Data(name), m_names(namesInput), m_type(type) {
    encodeContent();
  }

  void encodeContent() {
    setContentType(m_type);
    uint32_t contentType = getContentType();
    if (contentType == ContentTypeValue::UpdateManifest ||
        contentType == ContentTypeValue::FrameManifest) {
      if (m_names.empty()) {
        setContent(std::span<uint8_t>{});
      } else {
        setContent(
            makeNestedBlock(ndn::tlv::Content, m_names.begin(), m_names.end()));
      }
    }
  }

  // for data of content: segment
  // Content=segment object as tlv to be nested
  explicit Data(const ndn::Name &name, std::vector<uint8_t> segmentContent)
      : ndn::Data(name) {
    setContentType(ContentTypeValue::MainData);
    ndn::Block segContent =
        ndn::encoding::makeBinaryBlock(ndn::tlv::Content, segmentContent);
    setContent(segContent);
  }
  explicit Data(const ndn::Name &name, ndn::Block dataContent, uint32_t type)
      : ndn::Data(name) {
    setContentType(type);
    NDN_LOG_DEBUG("Data-------: " << dataContent.type());
    ndn::Block binaryBlock = ndn::encoding::makeBinaryBlock(
        ndn::tlv::Content, dataContent.value_begin(), dataContent.value_end());
    setContent(binaryBlock);
  }

  // for data of content: metaInfo(Json)
  Data(const ndn::Name &name, std::span<uint8_t> jsonContent)
      : ndn::Data(name) {
    setContentType(ContentTypeValue::Json);
    ndn::Block binaryBlock =
        ndn::encoding::makeBinaryBlock(ndn::tlv::Content, jsonContent);
    setContent(binaryBlock);
  }

  void wireDecode(const ndn::Block &wire) {
    Data::wireDecode(wire);
    uint32_t contentType = getContentType();

    if (contentType == ContentTypeValue::UpdateManifest ||
        contentType == ContentTypeValue::FrameManifest) {
      m_names.clear();
      auto content = getContent();
      content.parse();
      for (const auto &del : content.elements()) {
        if (del.type() == ndn::tlv::Name) {
          m_names.emplace_back(del);
        } else if (ndn::tlv::isCriticalType(del.type())) {
          NDN_THROW(Error("Unexpected TLV-TYPE " + std::to_string(del.type()) +
                          " while decoding ManifestContent"));
        }
      }
    }
  }

private:
  uint32_t m_type;
  std::vector<ndn::Name> m_names;
};

} // namespace iceflow

#endif // ICEFLOW_CORE_ICEFLOW_DATA_HPP
