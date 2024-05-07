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

#ifndef ICEFLOW_PRODUCER_HPP
#define ICEFLOW_PRODUCER_HPP

#include "iceflow.hpp"

namespace iceflow {

/**
 * Allows for publishing data to `IceflowConsumer`s.
 *
 * The data is split across one or more `topicPartitions` whose indexes can be
 * passed as a constructor argument.
 */
class IceflowProducer {
public:
  IceflowProducer(std::shared_ptr<IceFlow> iceflow, const std::string &pubTopic,
                  const std::vector<uint64_t> &topicPartitions,
                  std::chrono::milliseconds publishInterval)
      : m_iceflow(iceflow), m_pubTopic(pubTopic),
        m_topicPartitions(topicPartitions), m_publishInterval(publishInterval),
        m_lastPublishTimePoint(std::chrono::steady_clock::now()) {

    checkPublishInterval(publishInterval);
    checkTopicPartitions(topicPartitions);

    if (auto validIceflow = m_iceflow.lock()) {
      std::function<QueueEntry(void)> popQueueValueCallback =
          std::bind(&IceflowProducer::popQueueValue, this);
      std::function<bool(void)> hasQueueValueCallback =
          std::bind(&IceflowProducer::hasQueueValue, this);
      std::function<std::chrono::time_point<std::chrono::steady_clock>(void)>
          getNextPublishTimePointCallback =
              std::bind(&IceflowProducer::getNextPublishTimePoint, this);
      std::function<void(void)> resetLastPublishTimepointCallback =
          std::bind(&IceflowProducer::resetLastPublishTimePoint, this);

      ProducerRegistrationInfo producerRegistration = {
          popQueueValueCallback,
          hasQueueValueCallback,
          getNextPublishTimePointCallback,
          resetLastPublishTimepointCallback,
      };

      m_subscriberId = validIceflow->registerProducer(producerRegistration);
    } else {
      throw std::runtime_error("Iceflow instance has already expired.");
    }
  }

  ~IceflowProducer() {
    if (auto validIceflow = m_iceflow.lock()) {
      validIceflow->deregisterProducer(m_subscriberId);
    }
  }

  void pushData(const std::vector<uint8_t> &data) { m_outputQueue.push(data); }

  void setPublishInterval(std::chrono::milliseconds publishInterval) {
    checkPublishInterval(publishInterval);

    m_publishInterval = publishInterval;
  }

  void setTopicPartitions(const std::vector<uint64_t> &topicPartitions) {
    checkTopicPartitions(topicPartitions);
    m_topicPartitions = topicPartitions;
  }

private:
  void checkPublishInterval(std::chrono::milliseconds publishInterval) {
    if (publishInterval.count() < 0) {
      throw std::invalid_argument("Publish interval has to be positive.");
    }
  }

  void checkTopicPartitions(const std::vector<uint64_t> &topicPartitions) {
    if (topicPartitions.empty()) {
      throw std::invalid_argument(
          "At least one topic partition has to be defined!");
    }
  }

  QueueEntry popQueueValue() {
    int partitionIndex = m_partitionCount++ % m_topicPartitions.size();
    uint64_t partitionNumber = m_topicPartitions[partitionIndex];
    auto data = m_outputQueue.waitAndPopValue();
    m_lastPublishTimePoint = std::chrono::steady_clock::now();

    return {
        m_pubTopic,
        partitionNumber,
        data,
    };
  };

  bool hasQueueValue() { return !m_outputQueue.empty(); }

  void resetLastPublishTimePoint() {
    m_lastPublishTimePoint = std::chrono::steady_clock::now();
  }

  std::chrono::time_point<std::chrono::steady_clock> getNextPublishTimePoint() {
    return m_lastPublishTimePoint + m_publishInterval;
  }

private:
  const std::weak_ptr<IceFlow> m_iceflow;
  const std::string m_pubTopic;
  RingBuffer<std::vector<uint8_t>> m_outputQueue;

  std::vector<uint64_t> m_topicPartitions;
  int m_partitionCount = 0;
  std::chrono::nanoseconds m_publishInterval;
  std::chrono::time_point<std::chrono::steady_clock> m_lastPublishTimePoint;

  uint64_t m_subscriberId;
};
} // namespace iceflow

#endif // ICEFLOW_PRODUCER_HPP
