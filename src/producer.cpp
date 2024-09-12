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
    std::shared_ptr<ndn::svs::SVSPubSub> svsPubSub,
    const std::string &nodePrefix, const std::string &syncPrefix,
    const std::string &downstreamEdgeName, uint32_t numberOfPartitions,
    std::optional<std::shared_ptr<CongestionReporter>> congestionReporter)
    : m_svsPubSub(svsPubSub), m_numberOfPartitions(numberOfPartitions),
      m_nodePrefix(nodePrefix),
      m_pubTopic(syncPrefix + "/" + downstreamEdgeName),
      m_randomNumberGenerator(std::mt19937(time(nullptr))),
      m_congestionReporter(congestionReporter) {

  setTopicPartitions(numberOfPartitions);
}

IceflowProducer::IceflowProducer(std::shared_ptr<ndn::svs::SVSPubSub> svsPubSub,
                                 const std::string &nodePrefix,
                                 const std::string &syncPrefix,
                                 const std::string &downstreamEdgeName,
                                 uint32_t numberOfPartitions)
    : IceflowProducer(svsPubSub, nodePrefix, syncPrefix, downstreamEdgeName,
                      numberOfPartitions, std::nullopt){};

IceflowProducer::IceflowProducer(
    std::shared_ptr<ndn::svs::SVSPubSub> svsPubSub,
    const std::string &nodePrefix, const std::string &syncPrefix,
    const std::string &downstreamEdgeName, uint32_t numberOfPartitions,
    std::shared_ptr<CongestionReporter> congestionReporter)
    : IceflowProducer(svsPubSub, nodePrefix, syncPrefix, downstreamEdgeName,
                      numberOfPartitions, std::optional(congestionReporter)){};

IceflowProducer::~IceflowProducer() {}

ndn::Name IceflowProducer::prepareDataName(const std::string &topic,
                                           uint32_t partitionNumber) {
  return ndn::Name(topic).appendNumber(partitionNumber);
}

void IceflowProducer::pushData(const std::vector<uint8_t> &data) {
  if (auto validSvsPubSub = m_svsPubSub.lock()) {
    auto partitionNumber = getNextPartitionNumber();
    auto dataID = prepareDataName(m_pubTopic, partitionNumber);
    auto sequenceNo = validSvsPubSub->publish(
        dataID, data, ndn::Name(m_nodePrefix), ndn::time::seconds(4));
    NDN_LOG_INFO("Publish: " << dataID << "/" << sequenceNo);
  }
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

void IceflowProducer::setTopicPartitions(uint64_t numberOfPartitions) {
  if (numberOfPartitions == 0) {
    throw std::invalid_argument(
        "At least one topic partition has to be defined!");
  }

  m_topicPartitions.clear();

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

} // namespace iceflow
