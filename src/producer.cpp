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

#include "producer.hpp"

namespace iceflow {

NDN_LOG_INIT(iceflow.IceflowProducer);

IceflowProducer::IceflowProducer(
    std::shared_ptr<IceFlow> iceflow, const std::string &pubTopic,
    uint32_t numberOfPartitions, std::chrono::milliseconds publishInterval,
    std::optional<std::shared_ptr<CongestionReporter>> congestionReporter)
    : m_iceflow(iceflow), m_numberOfPartitions(numberOfPartitions),
      m_pubTopic(pubTopic),
      m_lastPublishTimePoint(std::chrono::steady_clock::now()),
      m_randomNumberGenerator(std::mt19937(time(nullptr))),
      m_congestionReporter(congestionReporter) {

  setTopicPartitions(numberOfPartitions);
  setPublishInterval(publishInterval);

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

IceflowProducer::IceflowProducer(std::shared_ptr<IceFlow> iceflow,
                                 const std::string &pubTopic,
                                 uint32_t numberOfPartitions,
                                 std::chrono::milliseconds publishInterval)
    : IceflowProducer(iceflow, pubTopic, numberOfPartitions, publishInterval,
                      std::nullopt){};

IceflowProducer::IceflowProducer(
    std::shared_ptr<IceFlow> iceflow, const std::string &pubTopic,
    uint32_t numberOfPartitions, std::chrono::milliseconds publishInterval,
    std::shared_ptr<CongestionReporter> congestionReporter)
    : IceflowProducer(iceflow, pubTopic, numberOfPartitions, publishInterval,
                      std::optional(congestionReporter)){};

IceflowProducer::~IceflowProducer() {
  if (auto validIceflow = m_iceflow.lock()) {
    validIceflow->deregisterProducer(m_subscriberId);
  }
}

void IceflowProducer::pushData(const std::vector<uint8_t> &data) {
  m_outputQueue.push(data);
}

void IceflowProducer::saveTimestamp(
    std::chrono::steady_clock::time_point timestamp) {
  m_productionTimestamps.push_back(timestamp);

  cleanUpTimestamps(timestamp);
}

void IceflowProducer::cleanUpTimestamps(
    std::chrono::time_point<std::chrono::steady_clock> referenceTimepoint) {

  while (!m_productionTimestamps.empty()) {
    auto firstValue = m_productionTimestamps.front();

    auto timePassed = firstValue - referenceTimepoint;

    if (timePassed <= m_maxProductionTimestampAge) {
      break;
    }

    m_productionTimestamps.pop_front();
  }
}

void IceflowProducer::setPublishInterval(
    std::chrono::milliseconds publishInterval) {
  if (publishInterval.count() < 0) {
    throw std::invalid_argument("Publish interval has to be positive.");
  }

  m_publishInterval = publishInterval;
}

void IceflowProducer::setTopicPartitions(uint64_t numberOfPartitions) {
  if (numberOfPartitions == 0) {
    throw std::invalid_argument(
        "At least one topic partition has to be defined!");
  }

  for (uint64_t i = 0; i < numberOfPartitions; ++i) {
    m_topicPartitions.emplace_hint(m_topicPartitions.end(), i);
  }
}

uint32_t IceflowProducer::getProductionStats() {
  auto referenceTimestamp = std::chrono::steady_clock::now();

  cleanUpTimestamps(referenceTimestamp);

  return m_productionTimestamps.size();
}

uint32_t IceflowProducer::getNextPartitionNumber() {
  return m_randomNumberGenerator() % m_numberOfPartitions;
}

// TODO: Determine where to use this method.
void IceflowProducer::reportCongestion(CongestionReason congestionReason,
                                       const std::string &edgeName) {
  if (!m_congestionReporter.has_value()) {
    NDN_LOG_WARN(
        "Detected a congestion, but no congestion reporter is defined.");
    return;
  }

  auto congestionReporter = m_congestionReporter.value();

  congestionReporter->reportCongestion(congestionReason, edgeName);
}

QueueEntry IceflowProducer::popQueueValue() {
  auto data = m_outputQueue.waitAndPopValue();
  uint32_t partitionNumber = getNextPartitionNumber();
  m_lastPublishTimePoint = std::chrono::steady_clock::now();

  saveTimestamp(m_lastPublishTimePoint);

  return {
      m_pubTopic,
      partitionNumber,
      data,
  };
};

bool IceflowProducer::hasQueueValue() { return !m_outputQueue.empty(); }

void IceflowProducer::resetLastPublishTimePoint() {
  m_lastPublishTimePoint = std::chrono::steady_clock::now();
}

std::chrono::time_point<std::chrono::steady_clock>
IceflowProducer::getNextPublishTimePoint() {
  return m_lastPublishTimePoint + m_publishInterval;
}
} // namespace iceflow
