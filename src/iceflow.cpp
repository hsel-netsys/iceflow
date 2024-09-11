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

#include "ndn-cxx/util/logger.hpp"

#include "iceflow.hpp"

namespace iceflow {

NDN_LOG_INIT(iceflow.IceFlow);

IceFlow::IceFlow(const std::string &syncPrefix, const std::string &nodePrefix,
                 ndn::Face &face)
    : m_syncPrefix(syncPrefix), m_nodePrefix(nodePrefix), m_face(face) {
  ndn::svs::SecurityOptions secOpts(m_keyChain);

  ndn::svs::SVSPubSubOptions opts;

  m_svsPubSub = std::make_shared<ndn::svs::SVSPubSub>(
      ndn::Name(m_syncPrefix), ndn::Name(m_nodePrefix), m_face,
      std::bind(&IceFlow::onMissingData, this, _1), opts, secOpts);
};

void IceFlow::run() {
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
    NDN_LOG_INFO("Checking if producers are registered...");
    std::unique_lock lock(m_producerRegistrationMutex);
    m_producerRegistrationConditionVariable.wait(
        lock, [this] { return m_producersAvailable; });
    NDN_LOG_INFO("At least one producer is registered, continuing "
                 "publishing thread.");

    auto closestNextPublishTimePoint =
        std::chrono::time_point<std::chrono::steady_clock>::max();

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

void IceFlow::shutdown() { m_running = false; }

uint32_t IceFlow::subscribeToTopicPartition(
    const std::string &topic, uint32_t partitionNumber,
    std::function<void(std::vector<uint8_t>)> &pushDataCallback) {

  auto subscribedTopic = ndn::Name(topic).appendNumber(partitionNumber);

  auto subscriptionHandle = m_svsPubSub->subscribe(
      subscribedTopic, std::bind(&IceFlow::subscribeCallBack, this,
                                 pushDataCallback, std::placeholders::_1));

  NDN_LOG_INFO("Subscribed to " << subscribedTopic);

  return subscriptionHandle;
}

void IceFlow::unsubscribe(uint32_t subscriptionHandle) {
  m_svsPubSub->unsubscribe(subscriptionHandle);
}

void IceFlow::subscribeCallBack(
    const std::function<void(std::vector<uint8_t>)> &pushDataCallback,
    const ndn::svs::SVSPubSub::SubscriptionData &subData) {
  NDN_LOG_DEBUG("Producer Prefix: " << subData.producerPrefix << " ["
                                    << subData.seqNo << "] : " << subData.name
                                    << " : ");

  std::vector<uint8_t> data(subData.data.begin(), subData.data.end());

  pushDataCallback(data);
}

void IceFlow::onMissingData(
    const std::vector<ndn::svs::MissingDataInfo> &missing_data) {
  // TODO: Implement if needed
}

ndn::Name IceFlow::prepareDataName(const std::string &topic,
                                   uint32_t partitionNumber) {
  return ndn::Name(topic).appendNumber(partitionNumber);
}

void IceFlow::publishMsg(std::vector<uint8_t> payload, const std::string &topic,
                         uint32_t partitionNumber) {
  auto dataID = prepareDataName(topic, partitionNumber);
  auto sequenceNo = m_svsPubSub->publish(
      dataID, payload, ndn::Name(m_nodePrefix), ndn::time::seconds(4));
  NDN_LOG_INFO("Publish: " << dataID << "/" << sequenceNo);
}

uint32_t
IceFlow::registerProducer(ProducerRegistrationInfo producerRegistration) {
  std::lock_guard lock(m_producerRegistrationMutex);

  uint32_t producerId = m_nextProducerId++;
  m_producerRegistrations.insert({producerId, producerRegistration});

  if (!m_producersAvailable) {
    m_producersAvailable = true;
    NDN_LOG_INFO("Producer has been registered, resuming producer procedure.");
    m_producerRegistrationConditionVariable.notify_one();
  }

  return producerId;
}

void IceFlow::deregisterProducer(u_int64_t producerId) {
  std::lock_guard lock(m_producerRegistrationMutex);

  m_producerRegistrations.erase(producerId);

  if (m_producerRegistrations.empty()) {
    m_producersAvailable = false;
    m_producerRegistrationConditionVariable.notify_one();
  }
}

} // namespace iceflow
