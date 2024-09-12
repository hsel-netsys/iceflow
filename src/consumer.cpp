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

IceflowConsumer::IceflowConsumer(std::shared_ptr<ndn::svs::SVSPubSub> svsPubSub,
                                 const std::string &syncPrefix,
                                 const std::string &upstreamEdgeName)
    : m_svsPubSub(svsPubSub), m_subTopic(syncPrefix + "/" + upstreamEdgeName) {}

IceflowConsumer::~IceflowConsumer() { unsubscribeFromAllPartitions(); }

void IceflowConsumer::saveTimestamp() {
  auto timestamp = std::chrono::steady_clock::now();

  m_consumptionTimestamps.push_back(timestamp);

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

void IceflowConsumer::validatePartitionConfiguration(
    uint32_t numberOfPartitions, uint32_t consumerPartitionIndex,
    uint32_t totalNumberOfConsumers) {
  if (numberOfPartitions == 0) {
    throw std::invalid_argument(
        "At least one topic partition has to be defined!");
  }

  if (totalNumberOfConsumers <= consumerPartitionIndex) {
    throw std::invalid_argument(
        "The total number of consumers has to be larger "
        "than the consumerPartitionIndex.");
  }

  if (numberOfPartitions < totalNumberOfConsumers) {
    throw std::invalid_argument(
        "The numberOfPartitions has to be at least as large as the "
        "totalNumberOfConsumers.");
  }

  if (numberOfPartitions <= consumerPartitionIndex) {
    throw std::invalid_argument("The numberOfPartitions has to be at least "
                                "as large as the consumerPartitionIndex.");
  }
}

bool IceflowConsumer::repartition(std::vector<uint32_t> partitions) {
  unsubscribeFromAllPartitions();

  try {
    for (auto partition : partitions) {
      // TODO: Deal with the return value
      subscribeToTopicPartition(partition);
    }
  } catch (const std::runtime_error &error) {
    return false;
  }

  return true;
}

uint32_t IceflowConsumer::getConsumptionStats() {
  auto referenceTimestamp = std::chrono::steady_clock::now();

  cleanUpTimestamps(referenceTimestamp);

  return m_consumptionTimestamps.size();
}

void IceflowConsumer::subscribeCallBack(
    const ndn::svs::SVSPubSub::SubscriptionData &subData) {
  NDN_LOG_DEBUG("Producer Prefix: " << subData.producerPrefix << " ["
                                    << subData.seqNo << "] : " << subData.name
                                    << " : ");

  std::vector<uint8_t> data(subData.data.begin(), subData.data.end());

  saveTimestamp();

  // TODO: Discuss if this is the right way to handle this.
  if (m_consumerCallback) {
    m_consumerCallback.value()(data);
    return;
  }

  NDN_LOG_WARN("No consumer callback defined for upstream edge "
               << "TODO");
}

uint32_t IceflowConsumer::subscribeToTopicPartition(uint64_t topicPartition) {
  if (auto validSvsPubSub = m_svsPubSub.lock()) {
    // TODO: Do we actually need an internal queue here...?

    // TODO: Consider using subscribeToProducer here instead
    return validSvsPubSub->subscribe(
        m_subTopic, std::bind(&IceflowConsumer::subscribeCallBack, this,
                              std::placeholders::_1));
  } else {
    throw std::runtime_error("SVS instance has already expired.");
  }
}

void IceflowConsumer::unsubscribeFromAllPartitions() {
  if (auto validSvsPubSub = m_svsPubSub.lock()) {
    for (auto subscriptionHandle : m_subscriptionHandles) {
      validSvsPubSub->unsubscribe(subscriptionHandle.second);
    }
  }
  m_subscriptionHandles.clear();
}

void IceflowConsumer::setConsumerCallback(ConsumerCallback consumerCallback) {
  m_consumerCallback = std::optional(consumerCallback);
}

} // namespace iceflow
