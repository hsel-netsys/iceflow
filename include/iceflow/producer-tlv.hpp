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

#ifndef ICEFLOW_CORE_PRODUCER_TLV_HPP
#define ICEFLOW_CORE_PRODUCER_TLV_HPP

#include "PSync/partial-producer.hpp"
#include "boost/algorithm/string.hpp"

#include "constants.hpp"
#include "data.hpp"
#include "ringbuffer.hpp"

namespace iceflow {

class ProducerTlv {
public:
  ProducerTlv(const ndn::Name &syncPrefix,
              const std::string &userPrefixDataMain,
              const std::string &userPrefixDataManifest,
              const std::string &userPrefixAck, int nDataStreams,
              int publishInterval, int publishIntervalNew, int mapThreshold)
      : m_scheduler(m_face.getIoService()),
        m_producer(m_face, m_keyChain, 100, syncPrefix, userPrefixDataMain),
        m_userPrefixDataMain(userPrefixDataMain),
        m_userPrefixDataManifest(userPrefixDataManifest),
        m_userPrefixAck(userPrefixAck), m_nDataStreams(nDataStreams),
        m_interval(publishInterval), m_pubInterval(publishIntervalNew),
        m_mapThreshold(mapThreshold) {

    // Add user prefixes and schedule updates for them
    for (int i = 0; i < m_nDataStreams; i++) {
      ndn::Name updateName(userPrefixDataMain + "/" + std::to_string(i));
      // Add the user prefix to the producer
      m_producer.addUserNode(updateName);
    }
    for (int i = 0; i < m_nDataStreams; i++) {
      m_dataMainCountStream[i] = 1;
      m_dataManifestCountStream[i] = 1;
    }

    if (!m_flag) {
      store();
      m_flag = true;
    }
  }
  virtual ~ProducerTlv() = default;
  void runPro() {
    // for main data
    m_face.setInterestFilter(
        m_userPrefixDataMain,
        std::bind(&ProducerTlv::onInterestMainData, this, _1, _2),
        nullptr, // RegisterPrefixSuccessCallback is optional
        std::bind(&ProducerTlv::onRegisterFailed, this, _1, _2));
    // for manifest data
    m_face.setInterestFilter(
        m_userPrefixDataManifest,
        std::bind(&ProducerTlv::onInterestManifestData, this, _1, _2),
        nullptr, // RegisterPrefixSuccessCallback is optional
        std::bind(&ProducerTlv::onRegisterFailed, this, _1, _2));

    // for ACK m_userPrefixAck
    m_face.setInterestFilter(
        m_userPrefixAck, std::bind(&ProducerTlv::onInterestAck, this, _1, _2),
        nullptr, // RegisterPrefixSuccessCallback is optional
        std::bind(&ProducerTlv::onRegisterFailed, this, _1, _2));

    m_face.processEvents();
  }
  void doUpdate() {
    for (int i = 0; i < m_nDataStreams; i++) {
      if (m_updateSeq[i] != m_updateSeqPrev[i]) {
        int manifestId = m_updateSeqPrev[i] + 1;
        int manifestStream = i;
        int dataLength = m_updateSeq[i] - m_updateSeqPrev[i];

        std::pair<int, int> key = std::make_pair(manifestId, manifestStream);
        m_updatesAckManifestNew[std::make_pair(manifestId, manifestStream)] =
            dataLength;
        m_updatesAckManifest[manifestId] = dataLength;
        //					NDN_LOG_TRACE("manifest ID: " <<
        // manifestId
        //					                             <<
        //" number of data: " << dataLength);
        ndn::Name updateName = m_userPrefixDataMain + "/" + std::to_string(i);
        m_producer.publishName(updateName, m_updateSeq[i]);
        m_seqNo = m_producer.getSeqNo(updateName).value();
        NDN_LOG_INFO("Publish: " << updateName << "/" << m_seqNo);
        m_updateSeqPrev[i] = m_updateSeq[i];
      }
    }
    m_scheduler.schedule(ndn::time::milliseconds(m_pubInterval),
                         [this] { doUpdate(); });
  }

  void store() {
    if (m_dataMainStorage.size() <= m_mapThreshold) {
      if (!outputQueueBlock.empty()) {

        m_resultBlock = outputQueueBlock.waitAndPopValue();
        m_resultBlock.iceFlowBlockParse();
        std::vector<ndn::Name> resultNameList;
        std::string interestNameDataMain;

        // extract the sub-blocks
        // test block types and values
        // if value>ndn packet size split and create manifest and add names
        // producer should not know the data type
        // add block type to the data packet (onInterest) ??
        // two prefixes - one for main data and manifest data

        std::vector<ndn::Block> resultSubElements =
            m_resultBlock.getSubElements();
        // if the block consists of multiple objects it needs a manifest
        if (resultSubElements.size() > 1) {
          // test the size of each sub element and split if needed
          for (int i = 0; i < resultSubElements.size(); i++) {
            if (resultSubElements[i].value_size() >
                ndn::MAX_NDN_PACKET_SIZE * 0.7) {
              // need to split
              std::vector<uint8_t> blockValue(
                  resultSubElements[i].value_begin(),
                  resultSubElements[i].value_end());
              // split buffer
              std::vector<std::vector<uint8_t>> bufferListNew =
                  splitVector(blockValue, ndn::MAX_NDN_PACKET_SIZE * 0.7);
              // create sub block to save the type// so store blocks
              for (int j = 0; j < bufferListNew.size(); j++) {
                const std::string interestNameDataManifest =
                    m_userPrefixDataManifest + "/" + std::to_string(m_stream) +
                    "/" + std::to_string(m_dataManifestCountStream[m_stream]);
                ndn::Block segmentedData = ndn::encoding::makeBinaryBlock(
                    resultSubElements[i].type(), bufferListNew[j]);
                m_dataManifestStorage.emplace(
                    std::pair(interestNameDataManifest, segmentedData));
                resultNameList.emplace_back(
                    ndn::Name(interestNameDataManifest));
                m_dataManifestCountStream[m_stream]++;
              }
            } else {
              // store and add name to manifest
              // add data prefix to the config file
              const std::string interestNameDataManifest =
                  m_userPrefixDataManifest + "/" + std::to_string(m_stream) +
                  "/" + std::to_string(m_dataManifestCountStream[m_stream]);
              m_dataManifestStorage.emplace(
                  std::pair(interestNameDataManifest, resultSubElements[i]));
              resultNameList.emplace_back(ndn::Name(interestNameDataManifest));
              m_dataManifestCountStream[m_stream]++;
            }
          }
          // add the manifest to manifest storage
          // store the manifest holding names of one result
          // manifest prefix
          // serialize the manifest and place it in the update storage
          // create block from the manifest and store it
          interestNameDataMain =
              m_userPrefixDataMain + "/" + std::to_string(m_stream) + "/" +
              std::to_string(m_dataMainCountStream[m_stream]);
          NDN_LOG_DEBUG("Interest main: " << interestNameDataMain);
          m_frameNames.emplace(std::pair(interestNameDataMain, resultNameList));
          ndn::Block manifestBlock =
              makeNestedBlock(ContentTypeValue::MainData,
                              resultNameList.begin(), resultNameList.end());
          m_dataMainStorage.emplace(
              std::pair(interestNameDataMain, manifestBlock));
          m_dataMainCountStream[m_stream]++;
          m_updateSeq[m_stream]++;
        }
        // if one object
        else {
          if (resultSubElements[0].value_size() >
              ndn::MAX_NDN_PACKET_SIZE * 0.7) {
            std::vector<uint8_t> blockValue(resultSubElements[0].value_begin(),
                                            resultSubElements[0].value_end());
            // split buffer
            std::vector<std::vector<uint8_t>> bufferListNew =
                splitVector(blockValue, ndn::MAX_NDN_PACKET_SIZE * 0.7);
            // create sub block to save the type// so store blocks
            for (int j = 0; j < bufferListNew.size(); j++) {
              const std::string interestNameDataManifest =
                  m_userPrefixDataManifest + "/" + std::to_string(m_stream) +
                  "/" + std::to_string(m_dataManifestCountStream[m_stream]);
              ndn::Block segmentedData = ndn::encoding::makeBinaryBlock(
                  resultSubElements[0].type(), bufferListNew[j]);
              m_dataManifestStorage.emplace(
                  std::pair(interestNameDataManifest, segmentedData));
              resultNameList.emplace_back(ndn::Name(interestNameDataManifest));
              m_dataManifestCountStream[m_stream]++;
            }
            interestNameDataMain =
                m_userPrefixDataMain + "/" + std::to_string(m_stream) + "/" +
                std::to_string(m_dataMainCountStream[m_stream]);
            m_frameNames.emplace(
                std::pair(interestNameDataMain, resultNameList));
            ndn::Block manifestBlock =
                makeNestedBlock(ContentTypeValue::MainData,
                                resultNameList.begin(), resultNameList.end());
            m_dataMainStorage.emplace(
                std::pair(interestNameDataMain, manifestBlock));
            m_dataMainCountStream[m_stream]++;
            m_updateSeq[m_stream]++;
          } else {
            // if data don't need to be segmented --json
            interestNameDataMain =
                m_userPrefixDataMain + "/" + std::to_string(m_stream) + "/" +
                std::to_string(m_dataMainCountStream[m_stream]);
            m_dataMainStorage.emplace(
                std::pair(interestNameDataMain, resultSubElements[0]));
            m_dataMainCountStream[m_stream]++;
            m_updateSeq[m_stream]++;
          }
        }

        m_stream++;
        if (m_stream == m_nDataStreams) {
          m_stream = 0;
        }
        if (!m_flagtest) {
          doUpdate();
          m_flagtest = true;
        }
      }
    }

    m_scheduler.schedule(ndn::time::milliseconds(m_interval),
                         [this] { store(); });
  }

  std::vector<std::vector<uint8_t>>
  splitVector(std::vector<uint8_t> inputVector, unsigned segmentSize) {
    auto start = inputVector.begin();
    auto end = inputVector.end();
    std::vector<std::vector<uint8_t>> returnValue;

    while (start != end) {
      std::vector<uint8_t> segment;
      const auto numToCopy = std::min(
          static_cast<unsigned>(std::distance(start, end)), segmentSize);
      std::copy(start, start + numToCopy, std::back_inserter(segment));
      returnValue.push_back(std::move(segment));
      std::advance(start, numToCopy);
    }
    return returnValue;
  }

public:
  RingBuffer<Block> outputQueueBlock;

  // for frames manifest
private:
  void onInterestMainData(const ndn::InterestFilter &,
                          const ndn::Interest &interest) {
    NDN_LOG_DEBUG(">> Got  InterestMainData: " << interest.getName());
    auto var = m_dataMainStorage.find(interest.getName().toUri());
    if (var != m_dataMainStorage.end()) {
      //				NDN_LOG_INFO("Sending from origin");
      ndn::Block mainDataBlock = m_dataMainStorage[interest.getName().toUri()];
      auto mainDataPacket =
          Data(interest.getName(), mainDataBlock, mainDataBlock.type());

      mainDataPacket.setFreshnessPeriod(constants::PRODUCER_FRESHNESS_PERIOD);

      // Sign Data packet with default identity
      m_keyChain.sign(mainDataPacket);
      // Return Data packet to the requester
      m_face.put(mainDataPacket);
    }

    else {
      auto varBackup = m_dataMainStorageBackup.find(interest.getName().toUri());
      if (varBackup != m_dataMainStorageBackup.end()) {
        //					NDN_LOG_INFO("Sending from
        // backup");
        ndn::Block mainDataBlockBackup =
            m_dataMainStorageBackup[interest.getName().toUri()];
        auto mainDataPacketBackup =
            Data(interest.getName(), mainDataBlockBackup,
                 mainDataBlockBackup.type());

        mainDataPacketBackup.setFreshnessPeriod(
            constants::PRODUCER_FRESHNESS_PERIOD);
        // Sign Data packet with default identity
        m_keyChain.sign(mainDataPacketBackup);
        // Return Data packet to the requester
        m_face.put(mainDataPacketBackup);
      }
    }
  }
  void onInterestManifestData(const ndn::InterestFilter &,
                              const ndn::Interest &interest) {
    NDN_LOG_DEBUG(">> Got  InterestManifestData: " << interest.getName());
    auto var = m_dataManifestStorage.find(interest.getName().toUri());
    if (var != m_dataManifestStorage.end()) {
      //				NDN_LOG_INFO("Sending from origin");

      ndn::Block manifestDataBlock =
          m_dataManifestStorage[interest.getName().toUri()];
      // to send json as manifest data//this condition can be removed when we
      // have 2 onData functions at the consumer
      Data manifestDataPacket;
      if (manifestDataBlock.type() == ContentTypeValue::Json) {
        manifestDataPacket = Data(interest.getName(), manifestDataBlock,
                                  ContentTypeValue::ManifestData);
      } else {
        manifestDataPacket = Data(interest.getName(), manifestDataBlock,
                                  manifestDataBlock.type());
      }

      manifestDataPacket.setFreshnessPeriod(
          constants::PRODUCER_FRESHNESS_PERIOD);
      // Sign Data packet with default identity
      m_keyChain.sign(manifestDataPacket);
      // Return Data packet to the requester
      m_face.put(manifestDataPacket);
    }

    else {
      auto varBackup =
          m_dataManifestStorageBackup.find(interest.getName().toUri());
      if (varBackup != m_dataManifestStorageBackup.end()) {
        ndn::Block manifestDataBlock =
            m_dataManifestStorageBackup[interest.getName().toUri()];
        // to send json as manifest data//this condition can be removed when we
        // have 2 onData functions at the consumer
        Data manifestDataPacket;
        if (manifestDataBlock.type() == ContentTypeValue::Json) {
          manifestDataPacket = Data(interest.getName(), manifestDataBlock,
                                    ContentTypeValue::ManifestData);
        } else {
          manifestDataPacket = Data(interest.getName(), manifestDataBlock,
                                    manifestDataBlock.type());
        }

        manifestDataPacket.setFreshnessPeriod(
            constants::PRODUCER_FRESHNESS_PERIOD);
        // Sign Data packet with default identity
        m_keyChain.sign(manifestDataPacket);
        // Return Data packet to the requester
        m_face.put(manifestDataPacket);
      }
    }
  }

  void onInterestAck(const ndn::InterestFilter &,
                     const ndn::Interest &interest) {
    NDN_LOG_DEBUG(">> Got ACK : " << interest.getName());

    // get manifest id and stream number
    std::string s = interest.getName().toUri();
    std::vector<std::string> strs;
    boost::split(strs, s, boost::is_any_of("/"));

    // data count -- stream count
    std::pair<int, int> key = std::make_pair(stoi(strs[strs.size() - 1]),
                                             stoi(strs[strs.size() - 2]));

    // get the number of data items belonging to the manifest using manifest id
    // and stream number
    auto varNewTest2 = m_updatesAckManifestNew.find(key);
    if (varNewTest2 != m_updatesAckManifestNew.end()) {
      std::string framePrefix = m_userPrefixDataMain + "/" +
                                std::to_string(varNewTest2->first.second) + "/";
      //				NDN_LOG_INFO("Manifest id from ACK:"
      //						             <<
      // varNewTest2->first.first
      //						             << " Stream
      // count: " << varNewTest2->first.second
      //						             << " Data
      // count: " << varNewTest2->second);
      // delete all frames belonging to a manifest
      for (int i = 0; i < varNewTest2->second; i++) {
        std::string frameName =
            framePrefix + std::to_string(varNewTest2->first.first + i);
        //					NDN_LOG_INFO("Frame to delete: "
        //<< frameName);
        auto var_new = m_dataMainStorage.find(frameName);
        if (var_new != m_dataMainStorage.end()) {
          m_dataMainStorageBackup.insert(
              make_pair(frameName, m_dataMainStorage[frameName]));
          //						NDN_LOG_INFO("Type: " <<
          // m_dataMainStorage[frameName].type());

          // manifest data to be emptied
          if (m_dataMainStorage[frameName].type() ==
              ContentTypeValue::MainData) {
            // get the list of names, delete from manifest storage, add to
            // manifest storage backup
            //							NDN_LOG_INFO("List of names
            //in a manifest:
            //"
            //									             <<
            // m_frameNames[frameName].size());
            for (int j = 0; j < m_frameNames[frameName].size(); j++) {
              auto var = m_dataManifestStorage.find(
                  m_frameNames[frameName][j].toUri());
              if (var != m_dataManifestStorage.end()) {
                m_dataManifestStorageBackup.insert(make_pair(
                    m_frameNames[frameName][j].toUri(),
                    m_dataManifestStorage[m_frameNames[frameName][j].toUri()]));
                m_dataManifestStorage.erase(m_frameNames[frameName][j].toUri());
              }
            }
          }
          m_dataMainStorage.erase(frameName);
        }
      }
    }
  }

  void onRegisterFailed(const ndn::Name &prefix, const std::string &reason) {
    NDN_LOG_ERROR("ERROR: Failed to register prefix '"
                  << prefix << "' with the local forwarder (" << reason << ")");
    m_face.shutdown();
  }

private:
  ndn::Face m_face;
  ndn::KeyChain m_keyChain;
  ndn::Scheduler m_scheduler;
  std::string m_userPrefixDataMain;
  std::string m_userPrefixDataManifest;
  std::string m_userPrefixAck;

  psync::PartialProducer m_producer;
  int m_nDataStreams;
  int m_interval;
  int m_pubInterval;
  int m_stream = 0;
  uint64_t m_seqNo = 0;
  int m_mapThreshold;

  Block m_resultBlock;
  std::map<std::string, std::vector<ndn::Name>>
      m_frameNames; // for the manifest (or frame) -- here we store the
  // manifests that contain the names of seg & mI

  std::map<std::string, ndn::Block> m_dataMainStorageBackup;
  std::map<std::string, ndn::Block> m_dataMainStorage;
  std::map<std::string, ndn::Block> m_dataManifestStorage;
  std::map<std::string, ndn::Block> m_dataManifestStorageBackup;

  bool m_flag = false;
  bool m_flagtest = false;
  std::map<int, int> m_updateSeq;
  std::map<int, int> m_updateSeqPrev;

  std::map<int, int> m_dataMainCountStream;
  std::map<int, int> m_dataManifestCountStream;

  // ACK manifest
  std::map<int, int> m_updatesAckManifest;
  std::map<std::pair<int, int>, int> m_updatesAckManifestNew;
};

} // namespace iceflow

#endif // ICEFLOW_CORE_PRODUCER_TLV_HPP