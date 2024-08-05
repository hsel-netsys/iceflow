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

#include "consumer.hpp"

namespace iceflow {

NDN_LOG_INIT(iceflow.IceflowConsumer);

IceflowConsumer::IceflowConsumer(std::shared_ptr<IceFlow> iceflow,
                                 const std::string &subTopic,
                                 uint32_t numberOfPartitions,
                                 uint32_t consumerPartitionIndex,
                                 uint32_t totalNumberOfConsumers)
    : m_iceflow(iceflow), m_subTopic(subTopic),
      m_numberOfPartitions(numberOfPartitions),
      m_consumerPartitionIndex(consumerPartitionIndex),
      m_totalNumberOfConsumers(totalNumberOfConsumers) {

  setTopicPartitions(m_numberOfPartitions, m_consumerPartitionIndex,
                     m_totalNumberOfConsumers);
}

IceflowConsumer::~IceflowConsumer() { unsubscribeFromAllPartitions(); }

std::vector<uint8_t> IceflowConsumer::receiveData() {
  return m_inputQueue.waitAndPopValue();
}

/**
 * Indicates whether the queue of this IceflowConsumer contains data.
 */
bool IceflowConsumer::hasData() { return !m_inputQueue.empty(); }

void IceflowConsumer::setNumberOfPartitions(uint32_t numberOfPartitions) {
  setTopicPartitions(numberOfPartitions, m_consumerPartitionIndex,
                     m_totalNumberOfConsumers);
  m_numberOfPartitions = numberOfPartitions;
}

void IceflowConsumer::setConsumerPartitionIndex(
    uint32_t consumerPartitionIndex) {
  setTopicPartitions(m_numberOfPartitions, consumerPartitionIndex,
                     m_totalNumberOfConsumers);
  m_consumerPartitionIndex = consumerPartitionIndex;
}

void IceflowConsumer::setTotalNumberOfConsumers(
    uint32_t totalNumberOfConsumers) {
  setTopicPartitions(m_numberOfPartitions, m_consumerPartitionIndex,
                     totalNumberOfConsumers);
  m_totalNumberOfConsumers = totalNumberOfConsumers;
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

/**
 * Updates the topic partitions this IceflowConsumer is subscribed to.
 *
 * Before updating the topic partitions, the method validates the passed
 * arguments in order to ensure a valid consumer configuration.
 */
void IceflowConsumer::setTopicPartitions(uint32_t numberOfPartitions,
                                         uint32_t consumerPartitionIndex,
                                         uint32_t totalNumberOfConsumers) {
  validatePartitionConfiguration(numberOfPartitions, consumerPartitionIndex,
                                 totalNumberOfConsumers);

  // TODO: Document that consumer indexes must start at 0
  std::unordered_set<uint32_t> topicPartitions;
  for (uint32_t i = consumerPartitionIndex; i < numberOfPartitions;
       i += totalNumberOfConsumers) {
    topicPartitions.emplace_hint(topicPartitions.end(), i);
  }

  if (auto validIceflow = m_iceflow.lock()) {
    std::vector<decltype(m_subscriptionHandles)::key_type> handlesToErase;
    for (auto subscriptionHandle : m_subscriptionHandles) {
      auto topicPartition = subscriptionHandle.first;

      if (!topicPartitions.contains(topicPartition)) {
        validIceflow->unsubscribe(subscriptionHandle.second);
        handlesToErase.push_back(subscriptionHandle.first);
      }
    }

    for (auto handleToErase : handlesToErase) {
      m_subscriptionHandles.erase(handleToErase);
    }

    for (auto topicPartition : topicPartitions) {
      if (!m_subscriptionHandles.contains(topicPartition)) {
        auto subscriptionHandle = subscribeToTopicPartition(topicPartition);
        m_subscriptionHandles.emplace(topicPartition, subscriptionHandle);
      }
    }
  } else {
    throw std::runtime_error("Iceflow instance has already expired.");
  }
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
