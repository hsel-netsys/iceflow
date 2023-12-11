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

#ifndef ICEFLOW_ICEFLOWSUBBASE_HPP
#define ICEFLOW_ICEFLOWSUBBASE_HPP
#include "ndn-svs/security-options.hpp"
#include <ndn-svs/svspubsub.hpp>

#include "ringbuffer.hpp"

#include "logger.hpp"
#include <iostream>
#include <thread>

namespace iceflow {
class IceFlowSub {
public:
  IceFlowSub(const std::string &syncPrefix, const std::string &subTopic,
             const std::vector<int> &nTopic)
      : mTopic(subTopic), topicPartition(nTopic) {
    // Use HMAC signing for Sync Interests
    // Note: this is not generally recommended, but is used here for simplicity
    ndn::svs::SecurityOptions secOpts(m_keyChain);
    secOpts.interestSigner->signingInfo.setSigningHmacKey(
        "dGhpcyBpcyBhIHNlY3JldCBtZXNzYWdl");
    ndn::svs::SVSPubSubOptions opts;
    m_SvSCon = std::make_shared<ndn::svs::SVSPubSub>(
        ndn::Name(syncPrefix), ndn::Name(subTopic), ndnInterFace,
        std::bind(&IceFlowSub::fetchingStateVector, this, _1), opts, secOpts);
    subscribe();
  }

  virtual ~IceFlowSub() = default;

  void subscribe() {
    // Allows consumer to select particular topic partition
    auto subscribed = m_SvSCon->subscribe(mTopic, IceFlowSub::updateCallBack);
    std::cout << "Subscribed Result: " << subscribed << std::endl;
    //    for (int i : topicPartition) {
    //      auto Subscribe2Prefix = mTopic + "/" + std::to_string(i);
    //      std::cout << "Subscribing to " << Subscribe2Prefix << std::endl;
    //      auto subscribed = m_SvSCon->subscribe(mTopic,
    //      IceFlowSub::updateCallBack); std::cout << "Subscribed Result: " <<
    //      subscribed << std::endl;
    //    }
  }

public:
  RingBuffer<std::string> inputQueue;
  ndn::Face ndnInterFace;

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

  /** Callback on receiving a new State Vector from another node.
   * This will be called regardless of whether the missing data contains any
   * topics or producers that we are subscribed to.
   *
   * @param missing_data contains new state vector
   */
  void fetchingStateVector(
      const std::vector<ndn::svs::MissingDataInfo> &missing_data) {
    // Iterate over the entire difference set
    for (size_t i = 0; i < missing_data.size(); i++) {
      // Iterate over each new sequence number that we learned
      for (ndn::svs::SeqNo sequence = missing_data[i].low;
           sequence <= missing_data[i].high; ++sequence) {
        std::cout << "Current Seq: " << sequence << std::endl;
      }
    }
  }
  void sendInterest(const ndn::Name &prefix) {
    ndn::Interest interest(prefix);
    interest.setCanBePrefix(false);
    interest.setMustBeFresh(true);
    interest.setInterestLifetime(
        ndn::time::seconds(4)); // The default is 4 seconds
    NDN_LOG_INFO(">> Sending Interest " << interest.getName().toUri());
    ndnInterFace.expressInterest(
        interest, std::bind(&IceFlowSub::dataHandler, this, _1, _2),
        std::bind(&IceFlowSub::nackHandler, this, _1, _2),
        std::bind(&IceFlowSub::timeoutHandler, this, _1));
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
  void dataHandler(const ndn::Interest &interest, const ndn::Data &data) {
    if (data.hasContent()) {
      std::cout << "Data: " << data.getContent()
                << " Data Type:" << data.getContentType() << std::endl;
      std::string decryptedContent(
          reinterpret_cast<const char *>(data.getContent().value()),
          data.getContent().size());
      inputQueue.push(decryptedContent);
    }
  }

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
  std::shared_ptr<ndn::svs::SVSPubSub> m_SvSCon;
  const std::string mTopic;
  const std::vector<int> topicPartition;
  ndn::KeyChain m_keyChain;
};

} // namespace iceflow
#endif // ICEFLOW_ICEFLOWSUBBASE_HPP
