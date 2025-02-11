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

#include <ranges>

#include "ndn-cxx/util/logger.hpp"

#include "consumer.hpp"

namespace iceflow {

NDN_LOG_INIT(iceflow.IceflowConsumer);

IceflowConsumer::IceflowConsumer(
    std::shared_ptr<ndn::svs::SVSPubSub> svsPubSub,
    const std::string &syncPrefix, const std::string &upstreamEdgeName,
    std::optional<std::shared_ptr<CongestionReporter>> congestionReporter)
    : m_svsPubSub(svsPubSub), m_subTopic(syncPrefix + "/" + upstreamEdgeName),
      m_upstreamEdgeName(upstreamEdgeName),
      m_congestionReporter(congestionReporter) {
  m_idleSince = std::chrono::steady_clock::now();
};

IceflowConsumer::~IceflowConsumer() { unsubscribeFromAllPartitions(); }

void IceflowConsumer::saveTimestamp() {
  auto timestamp = std::chrono::steady_clock::now();

  m_consumptionTimestamps.push_back(timestamp);
  m_idleSince = timestamp;

  cleanUpTimestamps(timestamp);
}

void IceflowConsumer::cleanUpTimestamps(
    std::chrono::time_point<std::chrono::steady_clock> referenceTimepoint) {

  while (!m_consumptionTimestamps.empty()) {
    auto firstValue = m_consumptionTimestamps.front();

    auto timePassed = firstValue - referenceTimepoint;

    if (timePassed <= m_maxConsumptionAge) {
      break;
    }

    m_consumptionTimestamps.pop_front();
  }
}

bool IceflowConsumer::repartition(std::vector<uint32_t> partitions) {
  unsubscribeFromAllPartitions();

  try {
    for (auto partition : partitions) {
      subscribeToTopicPartition(partition);
    }
  } catch (const std::runtime_error &error) {
    return false;
  }

  return true;
}

uint64_t IceflowConsumer::determineIdleTime(
    std::chrono::time_point<std::chrono::steady_clock> referenceTimepoint) {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(referenceTimepoint -
                                                            m_idleSince)
          .count());
}

EdgeConsumptionStats IceflowConsumer::getConsumptionStats() {
  auto referenceTimestamp = std::chrono::steady_clock::now();
  auto idleTime = determineIdleTime(referenceTimestamp);

  cleanUpTimestamps(referenceTimestamp);

  return EdgeConsumptionStats{m_consumptionTimestamps.size(), idleTime};
}

void IceflowConsumer::subscribeCallBack(
    const ndn::svs::SVSPubSub::SubscriptionData &subData) {
  NDN_LOG_DEBUG("Producer Prefix: " << subData.producerPrefix << " ["
                                    << subData.seqNo << "] : " << subData.name
                                    << " : ");

  if (!m_consumerCallback) {
    NDN_LOG_WARN("No consumer callback defined for upstream edge "
                 << m_upstreamEdgeName);
    return;
  }

  auto originalData = subData.data;
  std::vector<uint8_t> data(originalData.begin(), originalData.end());

  m_consumerCallback.value()(data);
  saveTimestamp();
}

ndn::Name IceflowConsumer::prepareDataName(uint32_t partitionNumber) {
  return ndn::Name(m_subTopic).appendNumber(partitionNumber);
}

void IceflowConsumer::subscribeToTopicPartition(uint64_t topicPartition) {
  if (auto validSvsPubSub = m_svsPubSub.lock()) {
    // TODO: For now I got rid of the output queue. I guess we can discuss if
    //       we actually need one or if the consumer application's callback
    //       should always be invoked directly.

    auto dataId = prepareDataName(topicPartition);
    // TODO: Consider using subscribeToProducer here instead
    auto subscriptionHandle = validSvsPubSub->subscribe(
        dataId, std::bind(&IceflowConsumer::subscribeCallBack, this,
                          std::placeholders::_1));

    m_subscriptionHandles.push_back(subscriptionHandle);
  } else {
    throw std::runtime_error("SVS instance has already expired.");
  }
}

void IceflowConsumer::unsubscribeFromAllPartitions() {
  if (auto validSvsPubSub = m_svsPubSub.lock()) {
    for (auto subscriptionHandle : m_subscriptionHandles) {
      validSvsPubSub->unsubscribe(subscriptionHandle);
    }
  }
  m_subscriptionHandles.clear();
}

void IceflowConsumer::setConsumerCallback(ConsumerCallback consumerCallback) {
  m_consumerCallback = std::optional(consumerCallback);
}

// TODO: Determine where to use this method.
void IceflowConsumer::reportCongestion(CongestionReason congestionReason) {
  if (!m_congestionReporter) {
    NDN_LOG_WARN(
        "Detected a congestion, but no congestion reporter is defined.");
    return;
  }

  auto congestionReporter = m_congestionReporter.value();

  congestionReporter->reportCongestion(congestionReason, m_upstreamEdgeName);
}

} // namespace iceflow
