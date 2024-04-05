/*
 * Copyright 2024 The IceFlow Authors.
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

#ifndef ICEFLOW_HPP
#define ICEFLOW_HPP

#include "constants.hpp"
#include "data.hpp"
#include "logger.hpp"
#include "ndn-svs/security-options.hpp"
#include "ringbuffer.hpp"
#include <ndn-svs/svspubsub.hpp>

#include <iostream>
#include <thread>

namespace iceflow {

/**
 * Central building block for IceFlow-based consumers and producers.
 *
 * Will act as a consumer if a `subTopic` is defined and act as a consumer if
 * a `pubTopic` is defined.
 * If both topics are passed to the constructor, the instantiated IceFlow
 * object will act both as a producer _and_ a consumer.
 */
class IceFlow {
  // TODO: Convert into base class and derive consumer, producer, and hybrid
  // base classes

public:
  IceFlow(const std::string &syncPrefix, const std::string &nodePrefix,
          const std::optional<std::string> &subTopic,
          const std::optional<std::string> &pubTopic,
          const std::vector<int> &topicPartitions, ndn::Face &face,
          std::optional<int> publishInterval)
      : m_face(face), m_scheduler(face.getIoContext()),
        m_nodePrefix(nodePrefix), m_pubTopic(pubTopic), m_subTopic(subTopic),
        m_topicPartitions(topicPartitions), m_publishInterval(publishInterval) {

    ndn::svs::SecurityOptions secOpts(m_keyChain);

    ndn::svs::SVSPubSubOptions opts;

    m_svsPubSub = std::make_shared<ndn::svs::SVSPubSub>(
        ndn::Name(syncPrefix), ndn::Name(nodePrefix), m_face,
        std::bind(&IceFlow::onMissingData, this, _1), opts, secOpts);

    if (!subTopic.has_value()) {
      return;
    }

    for (auto topicPartition : m_topicPartitions) {
      auto subscribedTopic =
          ndn::Name(subTopic.value()).appendNumber(topicPartition);

      m_svsPubSub->subscribe(
          subscribedTopic,
          std::bind(&IceFlow::subscribeCallBack, this, std::placeholders::_1));
      std::cout << "Subscribed to " << subscribedTopic << std::endl;
    }
  }

  virtual ~IceFlow() = default;

public:
  void run() {
    std::thread svsThread([this] { m_face.processEvents(); });

    if (m_pubTopic.has_value()) {
      publishMsg();
    }

    svsThread.join();
  }

  void pushData(std::vector<uint8_t> &data) { m_outputQueue.push(data); }

  std::vector<uint8_t> receiveData() { return m_inputQueue.waitAndPopValue(); }

private:
  void subscribeCallBack(const ndn::svs::SVSPubSub::SubscriptionData &subData) {
    NDN_LOG_DEBUG("Producer Prefix: " << subData.producerPrefix << " ["
                                      << subData.seqNo << "] : " << subData.name
                                      << " : ");

    auto data = subData.data;
    m_inputQueue.push(std::vector<uint8_t>(data.begin(), data.end()));
  }

  void
  onMissingData(const std::vector<ndn::svs::MissingDataInfo> &missing_data) {
    // TODO: Implement if needed
  }

  ndn::Name prepareDataName() {
    int partitionIndex = m_partitionCount++ % m_topicPartitions.size();
    auto partitionNumber = m_topicPartitions[partitionIndex];

    return ndn::Name(m_pubTopic.value()).appendNumber(partitionNumber);
  }

  void publishMsg() {
    if (!m_outputQueue.empty()) {
      auto dataID = prepareDataName();
      auto payload = m_outputQueue.waitAndPopValue();
      auto sequenceNo = m_svsPubSub->publish(
          dataID, payload, ndn::Name(m_nodePrefix), ndn::time::seconds(4));
      NDN_LOG_INFO("Publish: " << dataID << "/" << sequenceNo);
    }

    if (m_publishInterval.has_value()) {
      m_scheduler.schedule(ndn::time::milliseconds(m_publishInterval.value()),
                           [this] { publishMsg(); });
    }
  }

private:
  ndn::Scheduler m_scheduler;
  ndn::KeyChain m_keyChain;
  ndn::Face &m_face;

  std::shared_ptr<ndn::svs::SVSPubSub> m_svsPubSub;

  const std::string m_nodePrefix;
  const std::optional<std::string> m_pubTopic;
  const std::optional<std::string> m_subTopic;
  std::optional<int> m_publishInterval;

  const std::vector<int> m_topicPartitions;
  int m_partitionCount = 0;

  RingBuffer<std::vector<uint8_t>> m_outputQueue;
  RingBuffer<std::vector<uint8_t>> m_inputQueue;
};

} // namespace iceflow

#endif // ICEFLOW_HPP
