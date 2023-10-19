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
#include "block.hpp"
#include "boost/algorithm/string.hpp"
#include "ndn-cxx/face.hpp"
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
              const std::string &subPrefixAck, const std::vector<int> &nSub,
              int inputThreshold)
      : m_subscriptionList(nSub), m_scheduler(m_face.getIoService()),
        m_ack(subPrefixAck), m_Sub(subPrefix),
        m_inputQueueThreshold(inputThreshold),
        m_consumer(syncPrefix, m_face,
                   std::bind(&ConsumerTlv::afterReceiveHelloData, this, _1),
                   std::bind(&ConsumerTlv::processSyncUpdate, this, _1),
                   m_subscriptionList.size(), 0.001, ndn::time::seconds(1),
                   ndn::time::milliseconds(1000)) {

    /**
     * Sends Hello Interest
     */
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
    NDN_LOG_DEBUG(">> Sending Interest ACK " << interest.getName().toUri());
    m_face.expressInterest(interest,
                           std::bind(&ConsumerTlv::onData, this, _1, _2),
                           std::bind(&ConsumerTlv::onNack, this, _1, _2),
                           std::bind(&ConsumerTlv::onTimeoutAck, this, _1));
  };

  RingBuffer<Block> *getInputBlockQueue() { return &m_inputBlockQueue; }

private:
  /**
   * Learns available upstreams and subscribes to desired stream
   *
   * Hello Sync Interest reply carries available stream names
   *
   * Finds out the desired stream name and subscribe to the stream
   * Sends Sync Interest to the individual stream
   *
   * @param streamNames contains the streams available in a sync group
   */

  void afterReceiveHelloData(const std::map<ndn::Name, uint64_t> &streamNames) {
    std::vector<ndn::Name> streamCollector;
    streamCollector.reserve(streamNames.size());
    NDN_LOG_DEBUG(
        "Number of Streams: " << streamNames.size()); // Need to discuss

    for (const auto &it : streamNames) {
      streamCollector.insert(streamCollector.end(), it.first);
      NDN_LOG_DEBUG("Available Streams: " << it.first);
    }

    for (int i : m_subscriptionList) {
      ndn::Name prefix = m_Sub + "/" + std::to_string(i);
      auto streamName = streamNames.find(prefix);
      if (streamName != streamNames.end()) {
        NDN_LOG_INFO("Subscribing to: " << prefix);
        m_consumer.addSubscription(prefix, streamName->second);
      } else {
        NDN_LOG_DEBUG("Topic not found");
      }
      m_consumer.sendSyncInterest();
    }
  }

  /**
   * Handles updates of the new Data object names and initiates Interests
   * sending
   *
   * Sync reply carries the newly produced sequence number of the data
   * objects of a stream
   *
   * Push the interest names to m_interestQueue (Interest Queue to maintain
   * FIFO)
   *
   * Starts interest sending -> 1st Interest (Anchor) / Interests
   *
   * @param updates sequence number of the data objects of the stream(prefix,
   * high seq, low seq, incoming face)
   */
  void processSyncUpdate(const std::vector<psync::MissingDataInfo> &updates) {

    for (const auto &update : updates) {

      auto availableNewData = update.highSeq - update.lowSeq;

      AckCount ackCount;
      for (uint64_t i = 0; i <= availableNewData; i++) {
        // Data can now be fetched using the prefix and sequence
        m_interestQueue.push(update.prefix.toUri() + "/" +
                             std::to_string(update.lowSeq + i));
        NDN_LOG_DEBUG("Update: " << update.prefix << "/"
                                 << (std::to_string(update.lowSeq + i)));
        ackCount.difference++;
      }

      ackCount.dataCount = 0;
      std::size_t stream_number = update.prefix.toUri().find_last_of("/\\");
      int stream = stoi(update.prefix.toUri().substr(stream_number + 1));

      std::pair<int, int> key = std::make_pair(update.lowSeq, stream);
      m_updatesAck[key] = ackCount;

      // send anchor interest
      if (m_flag == 0) {
        if (!m_interestQueue.empty()) {
          if (m_theoreticalWindowSize == 0) {
            m_theoreticalWindowSize++;
            m_step = 0;
          }
          sendNewInterest(1);
          m_flag++;
        }
      }
    }
  }

  /**
   * Sends the 1st interest, also known as the Anchor Interest.
   *
   *  An Anchor Interest is also sent when a restart of the
   *  pipelines after a pause of a DataFlow occurs.
   */
  void sendAnchor() {
    if ((m_inputBlockQueue.size() < m_inputQueueThreshold) && !m_flagNew &&
        m_theoreticalWindowSize == 0) {
      if (!m_interestQueue.empty()) {
        if (m_theoreticalWindowSize == 0) {
          m_theoreticalWindowSize++;
          m_step = 0;
        }
        sendNewInterest(1);
      }
    }

    auto duration = ndn::time::duration_cast<ndn::time::nanoseconds>(
        ndn::time::milliseconds(1));
    m_scheduler.schedule(duration, [this] { sendAnchor(); });
  }

  void storeContentBlock(ndn::Name manifestName, ndn::Block contentBlock) {
    auto manifestBlocks = m_manifestBlocks[manifestName];
    manifestBlocks.push_back(contentBlock);
  }

  void groupManifestDataByType(
      std::vector<std::vector<ndn::Block>> *splitManifestBlocks,
      ndn::Name manifestName) {
    auto manifestBlocks = m_manifestBlocks[manifestName];

    for (int manifestDataType : m_manifestDataTypes) {
      std::vector<ndn::Block> tmp;
      for (auto &manifestBlock : manifestBlocks) {
        if (manifestBlock.type() == manifestDataType) {
          tmp.push_back(manifestBlock);
        }
      }
      splitManifestBlocks->push_back(tmp);
    }
  }

  void aggregateManifestBlocks(
      Block *iceflowBlock,
      std::vector<std::vector<ndn::Block>> splitManifestBlocks) {
    for (auto &splitManifestBlock : splitManifestBlocks) {
      auto firstBlock = splitManifestBlock[0];

      if (splitManifestBlock.size() <= 1) {
        // push the block to Iceblock // for json
        iceflowBlock->pushBlock(firstBlock);
        continue;
      }

      std::vector<uint8_t> aggregatedData =
          aggregateSegments(splitManifestBlock);
      ndn::Block binaryBlock =
          ndn::encoding::makeBinaryBlock(firstBlock.type(), aggregatedData);
      iceflowBlock->pushBlock(binaryBlock);
    }
  }

  int determineManifestStreamCount(
      std::vector<std::string> manifestBlockNames) {
    return stoi(manifestBlockNames[manifestBlockNames.size() - 2]);
  }

  /**
   *
   */
  void determineManifestBlockNames(std::vector<std::string> manifestBlockNames,
                                   ndn::Name manifestName) {
    std::string frameSeq = manifestName.toUri();

    boost::split(manifestBlockNames, frameSeq, boost::is_any_of("/"));
  }

  void acknowledge(std::pair<int, int> manifestIndices) {
    auto updateAcknowledgment = m_updatesAck[manifestIndices];
    updateAcknowledgment.dataCount++;

    if (updateAcknowledgment.dataCount == updateAcknowledgment.difference) {
      sendAckManifest(manifestIndices);
    }
  }

  void handleFrameDataCompletion(ndn::Name manifestName) {
    Block iceflowBlock;
    std::vector<std::vector<ndn::Block>> splitManifestBlocks;
    std::vector<std::string> manifestBlockNames;

    groupManifestDataByType(&splitManifestBlocks, manifestName);
    aggregateManifestBlocks(&iceflowBlock, splitManifestBlocks);
    addBlockToInputQueue(iceflowBlock);
    determineManifestBlockNames(manifestBlockNames, manifestName);
    auto manifestIndices = determineManifestIndices(manifestBlockNames);
  }

  /**
   * Calculates the highest lower sequence number and the manifest stream count
   * from the vector of manifest block names given.
   *
   * @param manifestBlockNames The block names to process.
   * @returns A tuple containing the highest lower sequence number and the
   * manifest stream count.
   */
  std::pair<int, int>
  determineManifestIndices(std::vector<std::string> manifestBlockNames) {
    size_t manifestBlockNamesSize = manifestBlockNames.size();
    int manifestStreamCount = determineManifestStreamCount(manifestBlockNames);
    int dataCount = stoi(manifestBlockNames[manifestBlockNamesSize - 1]);
    int highestLowerSequenceNumber = 0;

    for (const auto &acknowledgement : m_updatesAck) {
      int lowerSequenceNumber = acknowledgement.first.first;
      int streamNumber = acknowledgement.first.second;
      if (lowerSequenceNumber <= dataCount &&
          streamNumber == manifestStreamCount &&
          highestLowerSequenceNumber < lowerSequenceNumber) {
        highestLowerSequenceNumber = lowerSequenceNumber;
      }
    }

    return std::pair<int, int>(highestLowerSequenceNumber, manifestStreamCount);
  }

  // TODO: Revisit naming
  void clearWindow(const ndn::Interest &interest) {
    auto interestName = interest.getName();
    m_window.remove(interestName);
    m_timedoutInterests.erase(interestName);
  }

  void sendNextWindowInterest() {
    if (m_flagData == 0 && !m_interestQueue.empty()) {
      if (m_theoreticalWindowSize == 0) {
        m_theoreticalWindowSize++;
        m_step = 0;
      }
      sendNewInterest(1);
      m_flagData++;
    }
  }

  bool contentTypeNotInManifest(uint32_t contentType) {
    return std::find(m_manifestDataTypes.begin(), m_manifestDataTypes.end(),
                  contentType) == m_manifestDataTypes.end();
  }

  void handleContentBlock(uint32_t contentType, const ndn::Interest &interest,
                          const ndn::Data &data) {
    auto content = data.getContent();
    ndn::Block contentBlock = ndn::encoding::makeBinaryBlock(
        contentType, content.value_begin(), content.value_end());
    auto interestName = interest.getName();
    auto interestUri = interestName.toUri();
    ndn::Name manifestName = m_segmentToFrame[interestUri];

    storeContentBlock(manifestName, contentBlock);

    if (contentTypeNotInManifest(contentType)) {
      m_manifestDataTypes.push_back(contentType);
    }

    int frameNumber = ++m_presentData[manifestName];
    bool frameDataComplete = frameNumber == m_names[manifestName].size();
    if (frameDataComplete) {
      handleFrameDataCompletion(manifestName);
    }

    clearWindow(interest);
    updateWindow(0);
    sendNextWindowInterest();
  }

  /**
   * Handles ndn::Data from upstream producers
   *
   * Pushes the data to the Input Queue of the Consumer
   * then gradually increases the data fetching/Interest sending rate
   * using Additive Increase Multiplicative Decrease(AIMD) Congestion Control.
   *
   * Manifest: Collection of segmented data objects name
   *     JSON: carries the analyzed results of the compute function of
   * 			upstreams.
   * @param interest named data interest
   * @param data actual data objects
   */
  void onData(const ndn::Interest &interest, const ndn::Data &data) {
    ndn::Block contentBlock;
    uint32_t contentType = data.getContentType();
    if (data.hasContent()) {

      switch (contentType) {

      case Manifest: {

        auto manifestNames = extractNamesFromData(data);

        for (const auto &manifestName : manifestNames) {
          NDN_LOG_DEBUG("Processing manifest name " << manifestName);
          std::string interestUri = interest.getName().toUri();
          m_interestQueue.push(manifestName);

          // save manifest name per segment -- change name later
          m_segmentToFrame[manifestName] = interestUri;

          // save total frame list (manifests with the list of names)
          m_names[interestUri].push_back(manifestName);
        }
      } break;

      case Json: {
        std::vector<std::string> jsonStorage;
        boost::split(jsonStorage, interest.getName().toUri(),
                     boost::is_any_of("/"));

        auto jsonData = Block(data.getContent(), data.getContentType());
        addBlockToInputQueue(jsonData);

        // need to update a manifest of json names and not only one data item
        /////////////////////////////////////////////////
        int manifestStreamCount = stoi(jsonStorage[jsonStorage.size() - 2]);

        // data sequence
        int dataCount = stoi(jsonStorage[jsonStorage.size() - 1]);

        int manifestID = 0;

        for (const auto &seqNum : m_updatesAck) {

          auto sequenceNumbers = seqNum.first;
          auto lowerSequenceNumber = sequenceNumbers.first;
          auto streamNumber = sequenceNumbers.second;

          NDN_LOG_DEBUG("First sequence number: "
                        << lowerSequenceNumber
                        << ", second sequence number: " << streamNumber);
          //           check the manifest and the stream
          if (lowerSequenceNumber <= dataCount &&
              streamNumber == manifestStreamCount) {
            if (manifestID < lowerSequenceNumber) {
              manifestID = lowerSequenceNumber;
            }
          }
        }

        auto manifestIndices = std::pair(manifestID, manifestStreamCount);

        m_updatesAck[manifestIndices].dataCount++;
        if (m_updatesAck[manifestIndices].dataCount ==
            m_updatesAck[manifestIndices].difference) {
          NDN_LOG_DEBUG("All data in the manifest received: "
                        << manifestID << "Data in manifest: "
                        << m_updatesAck[manifestIndices].difference
                        << "Stream: " << manifestStreamCount);
          sendAckManifest(manifestIndices);
        }
        ////////////////////////////////////////////////
      } break;

      case JsonManifest:
      case SegmentsInManifest: {
        handleContentBlock(contentType, interest, data);
        break;
      }
      }
    }
  }

  /**
   * Filters out the blocks in an ndn::Data object that represent NDN names and
   * returns them as a vector.
   */
  std::vector<ndn::Name> extractNamesFromData(const ndn::Data &data) {
    const auto &content = data.getContent();
    content.parse();

    std::vector<ndn::Name> manifestNames;
    // TODO: Refactor with function like std::copy_if
    for (const auto &block : content.elements()) {
      if (block.type() == ndn::tlv::Name) {
        manifestNames.emplace_back(block);
      }
    }

    return manifestNames;
  }

  /**
   * Sends Manifest Acknowledgment Interests to Producers
   *
   * @param seq
   * @param stream
   */
  void sendAckManifest(std::pair<int, int> foo) {
    int lowerSequenceNumber = foo.first;
    int manifestStreamCount = foo.second;

    ndn::Name ackInterest = m_ack + +"/" + std::to_string(manifestStreamCount) +
                            "/" + std::to_string(lowerSequenceNumber);
    NDN_LOG_DEBUG("ACK Name interest: " << ackInterest);
    sendInterestAck(ackInterest);
  }

  /**
   * Handles the merge of the segments of a Data
   *
   * @param data
   * @return
   */
  static std::vector<uint8_t> aggregateSegments(std::vector<ndn::Block> data) {
    std::vector<uint8_t> aggregatedData;
    for (int i = 0; i < data.size(); i++) {
      aggregatedData.insert(aggregatedData.end(),
                            std::make_move_iterator(data[i].value_begin()),
                            std::make_move_iterator(data[i].value_end()));
    }
    return aggregatedData;
  }

  /**
   * Pushes the ndn::Data to Input Queue
   * @param dataBlock
   */
  void addBlockToInputQueue(Block dataBlock) {
    m_inputBlockQueue.push(dataBlock);
  }

  /**
   * Handles negative ACK Interests
   * Triggers Interest retransmission
   * @param interest interest name that could not retrieve data
   * @param nack Network-level NACK packets
   */
  void onNack(const ndn::Interest &interest, const ndn::lp::Nack &nack) {
    NDN_LOG_DEBUG("Received Nack with reason " << nack.getReason());
    m_scheduler.schedule(ndn::time::milliseconds(200),
                         [this] { sendNewInterest(1); });
    NDN_LOG_DEBUG("Retransmission: " << interest.getName());
  }
  /**
   * Handles timed out Interest packets
   *
   * Triggers retransmission
   *
   * Updates Interest sending Window
   * @param interest timed out Interest Name
   */
  void onTimeout(const ndn::Interest &interest) {
    NDN_LOG_DEBUG("Timeout " << interest.getName());
    m_window.remove(interest.getName());
    addTimeOut(interest.getName());
    m_theoreticalWindowSize = m_theoreticalWindowSize / 2;
    m_step = 0;
    updateWindow(1);
  }

  /**
   * Handles Timed Out Ack Interest being sent to upstreams
   * @param interest timed out ACK Interest Name
   */
  void onTimeoutAck(const ndn::Interest &interest) {
    NDN_LOG_DEBUG("Timeout Ack" << interest.getName());
  }

  /**
   * Handles the addition of the Timed Out Interest to Interest Window
   * @param name timed out Interest Names
   */
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

  void resetWindow() {
    m_step = 0;
    if (m_inputQueueThreshold >= m_inputBlockQueue.size()) {
      m_theoreticalWindowSize =
          m_inputQueueThreshold - m_inputBlockQueue.size();
    } else {
      m_theoreticalWindowSize = 0;
    }
    m_flagNew = false;
    sendAnchor();
  }

  /**
   * Handles the Windowing for sending Interests
   * @param i
   */

  // TODO: Separate the Congestion Control Mechanism to another file
  void updateWindow(int i) {
    if (m_window.size() > m_theoreticalWindowSize) {
      // wait for ws to decrease
      m_step = 0;
      return;
    }

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
          resetWindow();
        }
        return;
      }
      // if shifting (or increase) and IQ starts to increase, need to decrease
      // ws to stay under threshold
      resetWindow();
    }
    // when both windows = 0
    if (m_theoreticalWindowSize == m_window.size() &&
        m_theoreticalWindowSize == 0 && m_flagNew) {
      m_theoreticalWindowSize++;
      m_step = 0;
      sendNewInterest(1);

    } else {
      sendNewInterest(
          m_theoreticalWindowSize -
          m_window.size()); // send 1 interest if shifting, more if ws increased
    }
  }

  void retransmitTimedOutInterests() {
    for (auto timedOutInterest : m_timedoutInterests) {
      const ndn::Name name = timedOutInterest.first;
      int timeOutCounter = ++timedOutInterest.second;
      m_window.push_back(name);
      sendInterest(name);
      if (timeOutCounter >= m_maxRetransmission) {
        m_timedoutInterests.erase(name);
      }
    }
  }

  void sendNewInterest(int windowSize) {
    retransmitTimedOutInterests();

    for (int i = 0; i < windowSize; i++) {
      if (m_interestQueue.empty()) {
        m_flag = 0;
        break;
      }

      ndn::Name interestName = m_interestQueue.waitAndPopValue();
      m_window.push_back(interestName);
      sendInterest(interestName);
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
  RingBuffer<ndn::Name> m_interestQueue;

  // for congestion control
  std::map<ndn::Name, int> m_timedoutInterests;
  std::list<ndn::Name> m_window;
  int m_theoreticalWindowSize = 0;
  int m_step = 0;
  int m_flag = 0;
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

  typedef int lowerSequenceNumber;
  typedef int streamNumber;

  /**
   * Maps pairs of lower sequence numbers and stream numbers to
   * data counts.
   */
  std::map<std::pair<lowerSequenceNumber, streamNumber>, AckCount> m_updatesAck;
};

} // namespace iceflow

#endif // ICEFLOW_CORE_CONSUMER_TLV_HPP
