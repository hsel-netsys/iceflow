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
#include "ringbuffer.hpp"

#include "ndn-svs/security-options.hpp"
#include "ndn-svs/svspubsub.hpp"

#include <chrono>
#include <iostream>
#include <thread>
#include <unordered_map>

namespace iceflow {

struct QueueEntry {
  std::string topic;
  uint64_t partitionNumber;
  std::vector<uint8_t> data;
};

struct ProducerRegistrationInfo {
  std::function<QueueEntry(void)> popQueueValue;
  std::function<bool(void)> hasQueueValue;
  std::function<std::chrono::time_point<std::chrono::steady_clock>(void)>
      getNextPublishTimePoint;
  std::function<void(void)> resetLastPublishTimepoint;
};

class IceflowProducer;
class IceflowConsumer;

/**
 * Central building block for IceFlow-based consumers and producers.
 *
 * Will act as a consumer if a `subTopic` is defined and act as a consumer if
 * a `pubTopic` is defined.
 * If both topics are passed to the constructor, the instantiated IceFlow
 * object will act both as a producer _and_ a consumer.
 */
class IceFlow {
public:
  /**
   * Generates a new IceFlow object from a `syncPrefix`, a `nodePrefix`, and a
   * custom `face`.
   */
  IceFlow(const std::string &syncPrefix, const std::string &nodePrefix,
          ndn::Face &face)
      : m_syncPrefix(syncPrefix), m_nodePrefix(nodePrefix), m_face(face) {
    ndn::svs::SecurityOptions secOpts(m_keyChain);

    ndn::svs::SVSPubSubOptions opts;

    m_svsPubSub = std::make_shared<ndn::svs::SVSPubSub>(
        ndn::Name(m_syncPrefix), ndn::Name(m_nodePrefix), m_face,
        std::bind(&IceFlow::onMissingData, this, _1), opts, secOpts);
  }

public:
  void run() {
    if (m_running) {
      throw std::runtime_error("Iceflow instance is already running!");
    }

    std::thread svsThread([this] {
      while (true) {
        try {
          m_face.processEvents(ndn::time::milliseconds(5000), true);
        } catch (std::exception &e) {
          NDN_LOG_ERROR("Error in event handling loop: " << e.what());
        }
      }
    });

    m_running = true;
    while (m_running) {
      auto closestNextPublishTimePoint =
          std::chrono::time_point<std::chrono::steady_clock>::max();

      {
        NDN_LOG_INFO("Checking if producers are registered...");
        std::unique_lock lock(m_producerRegistrationMutex);
        m_producerRegistrationConditionVariable.wait(
            lock, [this] { return m_producersAvailable; });
        NDN_LOG_INFO("At least one producer is registered, continuing "
                     "publishing thread.");
      }

      for (auto producerRegistrationTuple : m_producerRegistrations) {
        auto producerRegistration = std::get<1>(producerRegistrationTuple);

        auto nextPublishTimePoint =
            producerRegistration.getNextPublishTimePoint();
        auto timeUntilNextPublish =
            nextPublishTimePoint - std::chrono::steady_clock::now();

        if (nextPublishTimePoint < closestNextPublishTimePoint) {
          closestNextPublishTimePoint = nextPublishTimePoint;
        }

        if (timeUntilNextPublish.count() > 0) {
          continue;
        }

        if (producerRegistration.hasQueueValue()) {
          auto queueEntry = producerRegistration.popQueueValue();
          publishMsg(queueEntry.data, queueEntry.topic,
                     queueEntry.partitionNumber);
        }

        producerRegistration.resetLastPublishTimepoint();
      }

      auto minimalTimeUntilNextPublish =
          closestNextPublishTimePoint - std::chrono::steady_clock::now();

      if (minimalTimeUntilNextPublish.count() > 0) {
        NDN_LOG_INFO("Sleeping for " << minimalTimeUntilNextPublish.count()
                                     << " nanoseconds...");
        std::this_thread::sleep_for(minimalTimeUntilNextPublish);
      }
    }

    m_face.shutdown();

    svsThread.join();
  }

  void shutdown() { m_running = false; }

  friend IceflowConsumer;
  friend IceflowProducer;

private:
  uint32_t subscribeToTopicPartition(
      const std::string &topic, uint64_t partitionNumber,
      std::function<void(std::vector<uint8_t>)> &pushDataCallback) {

    auto subscribedTopic = ndn::Name(topic).appendNumber(partitionNumber);

    auto subscriptionHandle = m_svsPubSub->subscribe(
        subscribedTopic, std::bind(&IceFlow::subscribeCallBack, this,
                                   pushDataCallback, std::placeholders::_1));

    std::cout << "Subscribed to " << topic << std::endl;

    return subscriptionHandle;
  }

  void unsubscribe(uint32_t subscriptionHandle) {
    m_svsPubSub->unsubscribe(subscriptionHandle);
  }

  void subscribeCallBack(
      const std::function<void(std::vector<uint8_t>)> &pushDataCallback,
      const ndn::svs::SVSPubSub::SubscriptionData &subData) {
    NDN_LOG_DEBUG("Producer Prefix: " << subData.producerPrefix << " ["
                                      << subData.seqNo << "] : " << subData.name
                                      << " : ");

    std::vector<uint8_t> data(subData.data.begin(), subData.data.end());

    pushDataCallback(data);
  }

  void
  onMissingData(const std::vector<ndn::svs::MissingDataInfo> &missing_data) {
    // TODO: Implement if needed
  }

  ndn::Name prepareDataName(const std::string &topic,
                            uint64_t partitionNumber) {
    return ndn::Name(topic).appendNumber(partitionNumber);
  }

  void publishMsg(std::vector<uint8_t> payload, const std::string &topic,
                  uint64_t partitionNumber) {
    auto dataID = prepareDataName(topic, partitionNumber);
    auto sequenceNo = m_svsPubSub->publish(
        dataID, payload, ndn::Name(m_nodePrefix), ndn::time::seconds(4));
    NDN_LOG_INFO("Publish: " << dataID << "/" << sequenceNo);
  }

  uint64_t registerProducer(ProducerRegistrationInfo producerRegistration) {
    uint64_t producerId = m_nextProducerId++;
    m_producerRegistrations.insert({producerId, producerRegistration});

    if (!m_producersAvailable) {
      {
        std::lock_guard lock(m_producerRegistrationMutex);
        m_producersAvailable = true;
        NDN_LOG_INFO(
            "Producer has been registered, resuming producer procedure.");
      }
      m_producerRegistrationConditionVariable.notify_one();
    }

    return producerId;
  }

  void deregisterProducer(u_int64_t producerId) {

    m_producerRegistrations.erase(producerId);

    if (m_producerRegistrations.empty()) {
      {
        std::lock_guard lock(m_producerRegistrationMutex);
        m_producersAvailable = false;
      }
      m_producerRegistrationConditionVariable.notify_one();
    }
  }

private:
  ndn::KeyChain m_keyChain;
  ndn::Face &m_face;

  std::mutex m_producerRegistrationMutex;
  std::condition_variable m_producerRegistrationConditionVariable;
  bool m_producersAvailable = false;

  bool m_running = false;

  std::shared_ptr<ndn::svs::SVSPubSub> m_svsPubSub;

  const std::string m_nodePrefix;
  const std::string m_syncPrefix;

  std::unordered_map<uint64_t, ProducerRegistrationInfo>
      m_producerRegistrations;

  uint64_t m_nextProducerId = 0;
};

} // namespace iceflow

#endif // ICEFLOW_HPP
