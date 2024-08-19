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

IceflowConsumer::IceflowConsumer(std::shared_ptr<IceFlow> iceflow,
                                 const std::string &subTopic,
                                 std::vector<uint32_t> partitions)
    : m_iceflow(iceflow), m_subTopic(subTopic), m_partitions(partitions) {

  repartition(partitions);
}

IceflowConsumer::~IceflowConsumer() { unsubscribeFromAllPartitions(); }

std::vector<uint8_t> IceflowConsumer::receiveData() {
  auto value = m_inputQueue.waitAndPopValue();

  // TODO: Discuss if this should be put elsewhere
  saveTimestamp();

  return value;
}

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

/**
 * Indicates whether the queue of this IceflowConsumer contains data.
 */
bool IceflowConsumer::hasData() { return !m_inputQueue.empty(); }

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

uint32_t IceflowConsumer::subscribeToTopicPartition(uint64_t topicPartition) {
  if (auto validIceflow = m_iceflow.lock()) {

    std::function<void(std::vector<uint8_t>)> pushDataCallback =
        std::bind(&RingBuffer<std::vector<uint8_t>>::push, &m_inputQueue,
                  std::placeholders::_1);

    return validIceflow->subscribeToTopicPartition(m_subTopic, topicPartition,
                                                   pushDataCallback);
  } else {
    throw std::runtime_error("Iceflow instance has already expired.");
  }
}

void IceflowConsumer::unsubscribeFromAllPartitions() {
  if (auto validIceflow = m_iceflow.lock()) {
    for (auto subscriptionHandle : m_subscriptionHandles) {
      validIceflow->unsubscribe(subscriptionHandle.second);
    }
  }
  m_subscriptionHandles.clear();
}
} // namespace iceflow
