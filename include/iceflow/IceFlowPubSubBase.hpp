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

#include "ndn-svs/security-options.hpp"
#include "ndn-svs/svspubsub.hpp"
#include "ndn-svs/svsync.hpp"
#include "ringbuffer.hpp"

#include "logger.hpp"
#include <iostream>
#include <thread>
namespace iceflow {

class IceFlowPubSub {

public:
  IceFlowPubSub(const std::string &syncPrefix, const std::string &topic,
                const std::vector<int> &nTopic)
      : mTopic(topic), topicPartition(nTopic.size()) {
    // Use HMAC signing for Sync Interests
    // Note: this is not generally recommended, but is used here for simplicity
    ndn::svs::SecurityOptions secOpts(m_keyChain);
    secOpts.interestSigner->signingInfo.setSigningHmacKey(
        "dGhpcyBpcyBhIHNlY3JldCBtZXNzYWdl");

    // Sign data packets using SHA256 (for simplicity)
    secOpts.dataSigner->signingInfo.setSha256Signing();

    // Do not fetch publications older than 10 seconds
    ndn::svs::SVSPubSubOptions opts;
    opts.UseTimestamp = true;
    opts.MaxPubAge = ndn::time::milliseconds(10000);

    m_svsps = std::make_shared<ndn::svs::SVSPubSub>(
        ndn::Name(syncPrefix), ndn::Name(topic), ndnInterFace,
        std::bind(&IceFlowPubSub::fetchingStateVector, this, _1), opts,
        secOpts);

    // Allows consumer to select particular topic partition
    for (int i = 0; i < topicPartition; i++) {
      auto Subscribe2Partition = topic + std::to_string(nTopic[i]);
      auto Subscribe2Prefix = mTopic + Subscribe2Partition;
      m_svsps->subscribe(Subscribe2Prefix, IceFlowPubSub::updateCallBack);

      // Start processing
      run();
    }
  }
	virtual ~IceFlowPubSub() = default;
public:
  RingBuffer<std::string> outputQueue;
  RingBuffer<std::string> inputQueue;

  void run() {
    std::vector<std::thread> processing_threads;

    processing_threads.emplace_back([this] { ndnInterFace.processEvents(); });
    processing_threads.emplace_back([this] { publishMsg(); });
    //    int threadCounter = 0;
    for (auto &thread : processing_threads) {
      //		  NDN_LOG_INFO("Thread " << threadCounter++ << "
      // started");
      thread.join();
    }
  }

  /** Callback on receiving a new State Vector from another node.
   * This will be called regardless of whether the missing data contains any
   * topics or producers that we are subscribed to.
   *
   * @param missing_data contains new state vector
   */
  void fetchingStateVector(
      const std::vector<ndn::svs::MissingDataInfo> &missing_data) {
    for (auto &updates : (missing_data)) {
      std::cout << "Low Seq" << updates.low << std::endl;
      std::cout << "High Seq" << updates.high << std::endl;
    }
  }

  /**
   *
   * @param subData
   */
  static void
  updateCallBack(const ndn::svs::SVSPubSub::SubscriptionData &subData) {
    std::string content(reinterpret_cast<const char *>(subData.data.data()),
                        subData.data.size());
    std::cout << subData.producerPrefix << " [" << subData.seqNo
              << "] : " << subData.name << " : ";
    if (content.length() > 200) {
      std::cout << "[LONG] " << content.length() << " bytes"
                << " [" << std::hash<std::string>{}(content) << "]";
    } else {
      std::cout << content;
    }
    std::cout << std::endl;
  }

  ndn::Name prepareDataName() {
    if (partitionCount < topicPartition) {
      partitionCount++;
      dataName = (mTopic +
                  std::to_string(
                      partitionCount)); // partition of a topic for publication
      dataName.appendTimestamp();       // and when
      return dataName;
    }
  }
  /**
   * Publish a message to the group
   */
  void publishMsg() {
    auto nameKey = prepareDataName();
    auto content = outputQueue.waitAndPopValue();
    storeFromQueue(nameKey, content);

    m_svsps->publish(dataName, ndn::make_span(reinterpret_cast<const uint8_t *>(
                                                  content.data()),
                                              content.size()));
  }

  void storeFromQueue(ndn::Name namedData, std::string data) {
//    dataStorage.emplace(std::pair(namedData, data));
	  dataStorage.insert(std::pair(namedData, data));
  }

  void sendInterest(const ndn::Name &prefix) {
    ndn::Interest interest(prefix);
    interest.setCanBePrefix(false);
    interest.setMustBeFresh(true);
    interest.setInterestLifetime(
        ndn::time::seconds(4)); // The default is 4 seconds
    NDN_LOG_INFO(">> Sending Interest " << interest.getName().toUri());
    ndnInterFace.expressInterest(
        interest, std::bind(&IceFlowPubSub::dataHandler, this, _1, _2),
        std::bind(&IceFlowPubSub::nackHandler, this, _1, _2),
        std::bind(&IceFlowPubSub::timeoutHandler, this, _1));
  }

  /**
   * Handles ndn::Data from upstream producers
   *
   * Pushes the data to the Input Queue of the Consumer
   * then gradually increases the data fetching/Interest sending rate
   * using Additive Increase Multiplicative Decrease(AIMD) Congestion Control.
   * @param interest named data interest
   * @param data actual data objects
   */
  void dataHandler(const ndn::Interest &interest, const ndn::Data &data) {}

  /**
   * Handles negative ACK Interests
   * Triggers Interest retransmission
   * @param interest interest name that could not retrieve data
   * @param nack Network-level NACK packets
   */
  void nackHandler(const ndn::Interest &interest, const ndn::lp::Nack &nack) {}

  /**
   * Handles timed out Interest packets
   *
   * Triggers retransmission
   *
   * Updates Interest sending Window
   * @param interest timed out Interest Name
   */
  void timeoutHandler(const ndn::Interest &interest) {}

private:
  /* data */
  std::shared_ptr<ndn::svs::SVSPubSub> m_svsps;
  ndn::Face ndnInterFace;
  ndn::KeyChain m_keyChain;
  const ndn::svs::UpdateCallback m_onUpdate;
  int partitionCount = 0;
  const std::string mTopic;
  const int topicPartition;
  ndn::Name topic;
  ndn::Name dataName;

  std::map<ndn::Name, std::string> dataStorage;
};
} // namespace iceflow
