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
#include "nlohmann/json.hpp"

#include "producer.hpp"

namespace iceflow {

NDN_LOG_INIT(iceflow.IceflowProducer);

IceflowProducer::IceflowProducer(
    std::shared_ptr<ndn::svs::SVSPubSub> svsPubSub,
    const std::string &nodePrefix, const std::string &syncPrefix,
    const std::string &downstreamEdgeName, uint32_t numberOfPartitions,
    std::optional<std::shared_ptr<CongestionReporter>> congestionReporter)
    : m_svsPubSub(svsPubSub), m_numberOfPartitions(numberOfPartitions),
      m_nodePrefix(nodePrefix), m_syncPrefix(syncPrefix),
      m_downstreamEdgeName(downstreamEdgeName),
      m_pubTopic(syncPrefix + "/" + downstreamEdgeName),
      m_randomNumberGenerator(std::mt19937(time(nullptr))),
      m_congestionReporter(congestionReporter) {

  setTopicPartitions(numberOfPartitions);
  setupBackchannel();
}

IceflowProducer::~IceflowProducer() {}

ndn::Name IceflowProducer::prepareDataName(uint32_t partitionNumber) {
  return ndn::Name(m_pubTopic).appendNumber(partitionNumber);
}

void IceflowProducer::setupBackchannel() {
  auto nodePrefix = m_nodePrefix;
  auto syncPrefix = m_syncPrefix;

  auto backChannelPrefixName = ndn::Name(m_pubTopic);
  backChannelPrefixName.append("backchannel");

  auto syncPrefixNameComponent = ndn::name::Component(m_syncPrefix);

  if (auto validSvsPubSub = m_svsPubSub.lock()) {
    auto dataStore = &validSvsPubSub->getSVSync().getDataStore();

    validSvsPubSub->subscribe(
        backChannelPrefixName,
        [dataStore, nodePrefix, syncPrefix, backChannelPrefixName](
            const ndn::svs::SVSPubSub::SubscriptionData &subscriptionData) {
          auto data = subscriptionData.data;

          try {
            auto sequenceNumberJson = nlohmann::json::parse(data);

            if (!sequenceNumberJson.is_number_integer()) {
              return;
            }

            uint64_t sequenceNumber =
                sequenceNumberJson.template get<uint64_t>();

            auto sequenceNumberNameComponent =
                ndn::name::Component::fromNumber(sequenceNumber);

            auto dataName = ndn::Name(nodePrefix);

            dataName.append(syncPrefix);
            dataName.append(sequenceNumberNameComponent);

            dataStore->erase(dataName);
          } catch (std::exception &e) {
            NDN_LOG_ERROR("Error encountered while handling backchannel input: "
                          << e.what());
            return;
          }
        });
  }
}

void IceflowProducer::pushData(const std::vector<uint8_t> &data) {
  if (auto validSvsPubSub = m_svsPubSub.lock()) {
    auto partitionNumber = getNextPartitionNumber();
    auto dataID = prepareDataName(partitionNumber);
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

uint64_t IceflowProducer::determineIdleTime(
    std::chrono::time_point<std::chrono::steady_clock> referenceTimepoint) {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(referenceTimepoint -
                                                            m_idleSince)
          .count());
}

EdgeProductionStats IceflowProducer::getProductionStats() {
  auto referenceTimestamp = std::chrono::steady_clock::now();
  auto idleTime = determineIdleTime(referenceTimestamp);

  cleanUpTimestamps(referenceTimestamp);

  return EdgeProductionStats{m_productionTimestamps.size(), idleTime};
}

uint32_t IceflowProducer::getNextPartitionNumber() {
  return m_randomNumberGenerator() % m_numberOfPartitions;
}

// TODO: Determine where to use this method.
void IceflowProducer::reportCongestion(CongestionReason congestionReason) {
  if (!m_congestionReporter) {
    NDN_LOG_WARN(
        "Detected a congestion, but no congestion reporter is defined.");
    return;
  }

  auto congestionReporter = m_congestionReporter.value();

  congestionReporter->reportCongestion(congestionReason, m_downstreamEdgeName);
}

} // namespace iceflow
