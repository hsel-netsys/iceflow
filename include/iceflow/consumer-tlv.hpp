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

#ifndef ICEFLOW_CORE_CONSUMER_TLV_HPP
#define ICEFLOW_CORE_CONSUMER_TLV_HPP

#include "PSync/consumer.hpp"
#include "boost/algorithm/string.hpp"
#include "ndn-cxx/face.hpp"

#include "block.hpp"
#include "ringbuffer.hpp"

namespace iceflow {

class ConsumerTlv {
public:
  /**
   * @brief Initialize consumer.o and start hello process
   *
   * 0.001 is the false positive probability of the bloom filter
   *
   * @param syncPrefix should be the same as producer
   * @param nSub vector of subscribe streams the consumer want to consume data
   * from
   */
  ConsumerTlv(const ndn::Name &syncPrefix, std::string &subPrefix,
              std::string &subPrefixAck, std::vector<int> nSub,
              int inputThreshold)
      : m_subscriptionList(nSub), m_scheduler(m_face.getIoService()),
        m_ack(subPrefixAck), m_Sub(subPrefix),
        m_inputQueueThreshold(inputThreshold),
        m_consumer(syncPrefix, m_face,
                   std::bind(&ConsumerTlv::afterReceiveHelloData, this, _1),
                   std::bind(&ConsumerTlv::processSyncUpdate, this, _1),
                   m_subscriptionList.size(), 0.001, ndn::time::seconds(1),
                   ndn::time::milliseconds(1000)) {

    // This starts the consumer.o side by sending a hello interest to the
    // producer When the producer responds with hello data,
    // afterReceiveHelloData is called
    m_consumer.sendHelloInterest();
  }

  virtual ~ConsumerTlv() = default;

  void runCon() { m_face.processEvents(); }

  void sendInterest(const ndn::Name &prefix) {
    ndn::Interest interest(prefix);
    interest.setCanBePrefix(false);
    interest.setMustBeFresh(true);
    interest.setInterestLifetime(
        ndn::time::seconds(4)); // The default is 4 seconds
    NDN_LOG_INFO(">> Sending Interest " << interest.getName().toUri());
    m_face.expressInterest(interest,
                           std::bind(&ConsumerTlv::onData, this, _1, _2),
                           std::bind(&ConsumerTlv::onNack, this, _1, _2),
                           std::bind(&ConsumerTlv::onTimeout, this, _1));
  };
  void sendInterestAck(const ndn::Name &prefix) {
    ndn::Interest interest(prefix);
    interest.setCanBePrefix(false);
    interest.setMustBeFresh(true);
    interest.setInterestLifetime(
        ndn::time::milliseconds(1)); // The default is 4 seconds
    NDN_LOG_INFO(">> Sending Interest ACK " << interest.getName().toUri());
    m_face.expressInterest(interest,
                           std::bind(&ConsumerTlv::onData, this, _1, _2),
                           std::bind(&ConsumerTlv::onNack, this, _1, _2),
                           std::bind(&ConsumerTlv::onTimeoutAck, this, _1));
  };

  RingBuffer<Block> *getInputBlockQueue() { return &m_inputBlockQueue; }

private:
  void afterReceiveHelloData(const std::map<ndn::Name, uint64_t> &availSubs) {
    std::vector<ndn::Name> userStream;
    userStream.reserve(availSubs.size());
    NDN_LOG_INFO("Number of Topics: " << availSubs.size());
    for (const auto &it : availSubs) {

      userStream.insert(userStream.end(), it.first);
      NDN_LOG_INFO("Topic names: " << it.first);
    }
    for (int i = 0; i < m_subscriptionList.size(); ++i) {
      ndn::Name prefix = m_Sub + "/" + std::to_string(m_subscriptionList[i]);
      NDN_LOG_INFO("Subscribed: " << prefix);
      auto it = availSubs.find(prefix);
      if (it != availSubs.end()) {
        NDN_LOG_INFO("Subscribing to: " << prefix);
        m_consumer.addSubscription(prefix, it->second);
      } else {
        NDN_LOG_INFO("Topic not found");
      }
      // After setting the subscription list, send the sync interest
      // The sync interest contains the subscription list
      // When new data is received for any subscribed prefix, processSyncUpdate
      // is called
      m_consumer.sendSyncInterest();
    }
  }

  void processSyncUpdate(const std::vector<psync::MissingDataInfo> &updates) {

    for (const auto &update : updates) {

      auto diff = update.highSeq - update.lowSeq;

      AckCount test;
      NDN_LOG_INFO("Diff: " << update.prefix << "= " << diff);

      for (uint64_t i = 0; i <= diff; i++) {
        // Data can now be fetched using the prefix and sequence
        m_interestDeque.push(update.prefix.toUri() + "/" +
                             std::to_string(update.lowSeq + i));
        NDN_LOG_INFO("Update: " << update.prefix << "/"
                                << (std::to_string(update.lowSeq + i)));
        test.difference++;
        NDN_LOG_INFO("interest dequeue size: " << m_interestDeque.size());
      }
      test.dataCount = 0;

      std::size_t found = update.prefix.toUri().find_last_of("/\\");
      int stream = stoi(update.prefix.toUri().substr(found + 1));

      std::pair<int, int> key = std::make_pair(update.lowSeq, stream);
      m_updatesAck[key] = test;
      NDN_LOG_INFO("Manifest ID on update: "
                   << update.lowSeq << " Stream: " << stream
                   << " Data count: " << test.difference);

      // send anchor interest
      if (flag == 0) {
        if (!m_interestDeque.empty()) {
          if (m_theoreticalWindowSize == 0) {
            m_theoreticalWindowSize++;
            m_step = 0;
          }
          sendInterestNew(1);
          flag++;
        }
      }
    }
  }

  void sendAnchor() {
    if ((m_inputBlockQueue.size() < m_inputQueueThreshold) && !m_flagNew &&
        m_theoreticalWindowSize == 0) {
      if (!m_interestDeque.empty()) {
        if (m_theoreticalWindowSize == 0) {
          m_theoreticalWindowSize++;
          m_step = 0;
        }
        sendInterestNew(1);
      }
    }

    auto duration = ndn::time::duration_cast<ndn::time::nanoseconds>(
        ndn::time::milliseconds(1));
    m_scheduler.schedule(duration, [this] { sendAnchor(); });
  }

  void onData(const ndn::Interest &interest, const ndn::Data &data) {
    ndn::Block cont;
    uint32_t contentType = data.getContentType();
    if (data.hasContent()) {

      switch (contentType) {

      case MainData: {
        NDN_LOG_INFO("got Manifest");
        const auto &content = data.getContent();
        content.parse();
        std::vector<ndn::Name> manifestNames;
        for (const auto &del : content.elements()) {
          if (del.type() == ndn::tlv::Name) {
            manifestNames.emplace_back(del);
          }
        }

        NDN_LOG_INFO("Manifest size: " << manifestNames.size());
        for (int i = 0; i < manifestNames.size(); ++i) {
          NDN_LOG_INFO("Manifest names: " << manifestNames[i]);
          m_interestDeque.push(manifestNames[i]); // Add name to cc updates
          m_segmentToFrame[manifestNames[i]] =
              interest.getName().toUri(); // save manifest name per segment --
                                          // change name later
          m_names[interest.getName().toUri()].push_back(
              manifestNames[i]); // save total frame list (manifests with the
                                 // list of names)
        }
      } break;
      case Json: {
        std::vector<std::string> strs;
        boost::split(strs, interest.getName().toUri(), boost::is_any_of("/"));
        addBlockToInputQueue(Block(data.getContent(), data.getContentType()));
        // need to update a manifest of json names and not only one data item
        /////////////////////////////////////////////////
        // stream
        int manifestStreamCount = stoi(strs[strs.size() - 2]);
        // data sequence
        int dataCount = stoi(strs[strs.size() - 1]);
        // key is the manifest id a data object belongs to
        int key = 0;

        for (const auto &seqNum : m_updatesAck) {
          auto sequenceNumbers = seqNum.first;
          auto firstSequenceNumber = sequenceNumbers.first;
          auto secondSequenceNumber = sequenceNumbers.second;
          NDN_LOG_INFO("First sequence number: " << firstSequenceNumber
                                                 << ", second sequence number: "
                                                 << secondSequenceNumber);
          // check the manifest and the stream
          if (firstSequenceNumber <= dataCount &&
              secondSequenceNumber == manifestStreamCount) {
            if (key < firstSequenceNumber) {
              key = firstSequenceNumber;
            }
          }
        }
        m_updatesAck[std::pair(key, manifestStreamCount)].dataCount++;
        if (m_updatesAck[std::pair(key, manifestStreamCount)].dataCount ==
            m_updatesAck[std::pair(key, manifestStreamCount)].difference) {
          NDN_LOG_INFO(
              "All data in the manifest received: "
              << key << "Data in manifest: "
              << m_updatesAck[std::pair(key, manifestStreamCount)].difference
              << "Stream: " << manifestStreamCount);
          sendAckManifest(key, manifestStreamCount);
        }
        ////////////////////////////////////////////////
      } break;

      case ManifestData:
      case SegmentManifest: {
        cont = ndn::encoding::makeBinaryBlock(contentType,
                                              data.getContent().value_begin(),
                                              data.getContent().value_end());
      } break;
      }
    }

    // TODO: Should the following code also be executed for SegmentManifests?
    if (contentType == ManifestData || contentType == SegmentManifest) {
      ndn::Name frame =
          m_segmentToFrame[interest.getName().toUri()]; // get manifest name of
                                                        // this data
      NDN_LOG_INFO("Frame name: " << frame);
      m_presentData[frame]++;
      m_manifestBlocks[frame].push_back(cont); // store Block of the manifest
      // check the number of data types per manifest
      if (std::find(m_manifestDataTypes.begin(), m_manifestDataTypes.end(),
                    contentType) == m_manifestDataTypes.end()) {
        m_manifestDataTypes.push_back(contentType);
      }
      std::vector<std::vector<ndn::Block>> splitManifestBlocks;
      if (m_presentData[frame] ==
          m_names[frame].size()) { // if we get all data belonging to one frame

        // grouping manifest data according to type
        for (int i = 0; i < m_manifestDataTypes.size(); i++) {
          std::vector<ndn::Block> tmp;
          for (int j = 0; j < m_manifestBlocks[frame].size(); j++) {
            if (m_manifestBlocks[frame][j].type() == m_manifestDataTypes[i]) {
              tmp.push_back(m_manifestBlocks[frame][j]);
            }
          }
          splitManifestBlocks.push_back(tmp);
        }
        Block iceflowBlock;
        for (int i = 0; i < splitManifestBlocks.size(); i++) {
          if (splitManifestBlocks[i].size() > 1) { // for frames
            // here we have to aggregate
            std::vector<uint8_t> aggregatedData =
                aggregateSegments(splitManifestBlocks[i]);
            ndn::Block binaryBlock = ndn::encoding::makeBinaryBlock(
                splitManifestBlocks[i][0].type(), aggregatedData);
            iceflowBlock.pushBlock(binaryBlock);
          } else {
            // push the block to Iceblock // for json
            iceflowBlock.pushBlock(splitManifestBlocks[i][0]);
          }
        }
        addBlockToInputQueue(iceflowBlock); // push data to input queue

        std::string frameSeq = frame.toUri();

        // find the manifest that frame seq belongs to.
        NDN_LOG_INFO("Frame sequence: " << frameSeq);
        std::vector<std::string> strs;
        boost::split(strs, frameSeq, boost::is_any_of("/"));

        // stream
        int manifestStreamCount = stoi(strs[strs.size() - 2]);
        // data sequence
        int dataCount = stoi(strs[strs.size() - 1]);
        // key is the manifest id a data object belongs to
        int key = 0;

        for (const auto &seqNum : m_updatesAck) {
          // check the manifest and the stream
          if (seqNum.first.first <= dataCount &&
              seqNum.first.second == manifestStreamCount) {
            if (key < seqNum.first.first) {
              key = seqNum.first.first;
            }
          }
        }
        m_updatesAck[std::pair(key, manifestStreamCount)].dataCount++;
        if (m_updatesAck[std::pair(key, manifestStreamCount)].dataCount ==
            m_updatesAck[std::pair(key, manifestStreamCount)].difference) {
          NDN_LOG_INFO(
              "All data in the manifest received: "
              << key << "Data in manifest: "
              << m_updatesAck[std::pair(key, manifestStreamCount)].difference
              << "Stream: " << manifestStreamCount);
          sendAckManifest(key, manifestStreamCount);
        }
      }
    }

    m_window.remove(interest.getName());
    m_timedoutInterests.erase(interest.getName());
    updateWindow(0);

    if (m_flagData == 0) {
      if (!m_interestDeque.empty()) {
        NDN_LOG_INFO("Sending data interest");
        if (m_theoreticalWindowSize == 0) {
          m_theoreticalWindowSize++;
          m_step = 0;
        }
        sendInterestNew(1);
        m_flagData++;
      }
    }
  }

  void sendAckManifest(int seq, int stream) {
    ndn::Name ackNameInterest =
        m_ack + +"/" + std::to_string(stream) + "/" + std::to_string(seq);
    NDN_LOG_INFO("ACK Name interest: " << ackNameInterest);
    sendInterestAck(ackNameInterest);
  }

  static std::vector<uint8_t> aggregateSegments(std::vector<ndn::Block> data) {
    std::vector<uint8_t> aggregatedData;
    NDN_LOG_INFO("aggregate data size: " << data.size());
    for (int i = 0; i < data.size(); i++) {
      aggregatedData.insert(aggregatedData.end(),
                            std::make_move_iterator(data[i].value_begin()),
                            std::make_move_iterator(data[i].value_end()));
    }
    return aggregatedData;
  }

  void addBlockToInputQueue(Block dataBlock) {
    m_inputBlockQueue.push(dataBlock);
  }

  void onNack(const ndn::Interest &, const ndn::lp::Nack &nack) {
    NDN_LOG_INFO("Received Nack with reason " << nack.getReason());
    m_scheduler.schedule(ndn::time::milliseconds(200),
                         [this] { sendInterestNew(1); });
    NDN_LOG_INFO("Sent Again - -  ");
  }

  void onTimeout(const ndn::Interest &interest) {
    NDN_LOG_INFO("Timeout " << interest.getName());
    m_window.remove(interest.getName());
    addTimeOut(interest.getName());
    m_theoreticalWindowSize = m_theoreticalWindowSize / 2;
    m_step = 0;
    updateWindow(1);
  }
  void onTimeoutAck(const ndn::Interest &interest) {
    NDN_LOG_INFO("Timeout Ack" << interest.getName());
  }

  void addTimeOut(ndn::Name name) {
    // check if the interest timedout before
    if (m_timedoutInterests.find(name) == m_timedoutInterests.end()) {
      // not found -- add to timedout interest
      m_timedoutInterests.emplace(std::pair(name, 1));
    } else {
      // found -- increment the count of timeout
      m_timedoutInterests[name]++;
    }
  }
  void updateWindow(int i) {
    NDN_LOG_INFO("update window");
    if (m_window.size() > m_theoreticalWindowSize) {
      // wait for ws to decrease
      m_step = 0;
    } else {
      if (i == 0) {
        m_step++;
        if (m_step == m_theoreticalWindowSize) {
          // shifting
          m_step = 0;
          m_flagNew = true;
          if ((m_inputBlockQueue.size() + m_theoreticalWindowSize) <
              m_inputQueueThreshold) {
            // increase
            m_theoreticalWindowSize++;
          }
          // if IQ increased -- keep it under threshold
          else if ((m_inputBlockQueue.size() + m_theoreticalWindowSize) >
                   m_inputQueueThreshold) {
            m_step = 0;
            if (m_inputQueueThreshold >= m_inputBlockQueue.size()) {
              m_theoreticalWindowSize =
                  m_inputQueueThreshold - m_inputBlockQueue.size();
            } else if (m_inputQueueThreshold < m_inputBlockQueue.size()) {
              m_theoreticalWindowSize = 0;
            }
            m_flagNew = false;
            sendAnchor();
          }

        }
        // if shifting (or increase) and IQ starts to increase, need to decrease
        // ws to stay under threshold
        else {
          if (m_inputQueueThreshold >= m_inputBlockQueue.size()) {
            m_step = 0;
            m_theoreticalWindowSize =
                m_inputQueueThreshold - m_inputBlockQueue.size();
          } else if (m_inputQueueThreshold < m_inputBlockQueue.size()) {
            m_step = 0;
            m_theoreticalWindowSize = 0;
          }
          m_flagNew = false;
          sendAnchor();
        }
      }
      // when both windows = 0
      if (m_theoreticalWindowSize == m_window.size() &&
          m_theoreticalWindowSize == 0 && m_flagNew) {
        m_theoreticalWindowSize++;
        m_step = 0;
        sendInterestNew(1);

      } else {
        sendInterestNew(
            m_theoreticalWindowSize -
            m_window
                .size()); // send 1 interest if shifting, more if ws increased
      }
    }
    NDN_LOG_INFO("Input threshold: " << m_inputBlockQueue.size() +
                                            m_theoreticalWindowSize);
  }

  void sendInterestNew(int n) {
    auto itr = m_timedoutInterests.begin();
    for (int(i) = 0; (i) < n; ++(i)) {
      // retransmissions first
      if (!m_timedoutInterests.empty()) {
        NDN_LOG_INFO("retransmission");
        itr->second++;
        m_window.push_back(itr->first);
        sendInterest(itr->first);
        if (itr->second >= m_maxRetransmission) {
          m_timedoutInterests.erase(itr++);
        } else {
          itr++;
        }
        if (itr == m_timedoutInterests.end()) {
          itr = m_timedoutInterests.begin();
        }

      } else {
        if (!m_interestDeque.empty()) {
          NDN_LOG_INFO("transmission");
          m_tmp = m_interestDeque.waitAndPopValue();
          m_window.push_back(m_tmp);
          sendInterest(m_tmp);
        } else {
          flag = 0;
        }
      }
    }
  }

private:
  RingBuffer<Block> m_inputBlockQueue;
  ndn::Face m_face;
  std::vector<int> m_subscriptionList;
  std::string m_Sub;
  std::string m_ack;
  ndn::Scheduler m_scheduler;
  int m_inputQueueThreshold;
  psync::Consumer m_consumer;
  RingBuffer<ndn::Name> m_interestDeque;

  // for congestion control
  std::map<ndn::Name, int> m_timedoutInterests;
  std::list<ndn::Name> m_window;
  int m_theoreticalWindowSize = 0;
  int m_step = 0;
  ndn::Name m_tmp;
  int flag = 0;
  int m_flagData = 0;
  bool m_flagNew = true;
  int m_maxRetransmission = 2;

  std::map<ndn::Name, std::vector<ndn::Name>> m_names;
  std::map<ndn::Name, std::vector<ndn::Block>> m_manifestBlocks;
  std::vector<int> m_manifestDataTypes;
  std::map<ndn::Name, ndn::Name> m_segmentToFrame; // segment/metaInfo -- Frame
  std::map<ndn::Name, int> m_presentData; // frame, count of data arrived

  struct AckCount {
    int difference = 0;
    int dataCount = 0;
  };
  // lowerSeqnum, stream, diff, available data
  std::map<std::pair<int, int>, AckCount> m_updatesAck;
};

} // namespace iceflow

#endif // ICEFLOW_CORE_CONSUMER_TLV_HPP
