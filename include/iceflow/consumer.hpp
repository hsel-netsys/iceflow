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

#ifndef ICEFLOW_CONSUMER_HPP
#define ICEFLOW_CONSUMER_HPP

#include "ndn-svs/svspubsub.hpp"

#include <chrono>
#include <unordered_map>
#include <vector>

#include "congestion-reporter.hpp"
#include "ringbuffer.hpp"

namespace iceflow {

/**
 * Allows for subscribing to data published by `IceflowProducer`s.
 */
class IceflowConsumer {

public:
  IceflowConsumer(std::shared_ptr<ndn::svs::SVSPubSub> svsPubSub,
                  const std::string &subTopic,
                  std::vector<uint32_t> partitions);

  ~IceflowConsumer();

  std::vector<uint8_t> receiveData();

  /**
   * Indicates whether the queue of this IceflowConsumer contains data.
   */
  bool hasData();

  bool repartition(std::vector<uint32_t> partitions);

  std::vector<u_int32_t> getPartitions();

  u_int32_t getConsumptionStats();

private:
  void validatePartitionConfiguration(uint32_t numberOfPartitions,
                                      uint32_t consumerPartitionIndex,
                                      uint32_t totalNumberOfConsumers);

  uint32_t subscribeToTopicPartition(
      const std::string &topic, uint32_t partitionNumber,
      std::function<void(std::vector<uint8_t>)> &pushDataCallback);

  void subscribeCallBack(
      const std::function<void(std::vector<uint8_t>)> &pushDataCallback,
      const ndn::svs::SVSPubSub::SubscriptionData &subData);

  /**
   * Updates the topic partitions this IceflowConsumer is subscribed to.
   *
   * Before updating the topic partitions, the method validates the passed
   * arguments in order to ensure a valid consumer configuration.
   */
  void setTopicPartitions(uint32_t numberOfPartitions,
                          uint32_t consumerPartitionIndex,
                          uint32_t totalNumberOfConsumers);

  uint32_t subscribeToTopicPartition(uint64_t topicPartition);

  void unsubscribeFromAllPartitions();

  void saveTimestamp();

  void cleanUpTimestamps(
      std::chrono::time_point<std::chrono::steady_clock> referenceTimepoint);

private:
  const std::weak_ptr<ndn::svs::SVSPubSub> m_svsPubSub;
  const std::string m_subTopic;

  std::vector<uint32_t> m_partitions;

  std::unordered_map<uint32_t, uint32_t> m_subscriptionHandles;

  RingBuffer<std::vector<uint8_t>> m_inputQueue;

  std::deque<std::chrono::time_point<std::chrono::steady_clock>>
      m_consumptionTimestamps;

  // TODO: Make configurable
  std::chrono::seconds m_maxConsumptionAge = std::chrono::seconds(1);
};
} // namespace iceflow

#endif // ICEFLOW_CONSUMER_HPP
