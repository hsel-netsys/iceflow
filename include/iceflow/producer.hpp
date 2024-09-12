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

#include "ndn-svs/svspubsub.hpp"

#include <random>
#include <unordered_set>

#include <time.h>

#ifdef USE_GRPC

#include "node-executor.grpc.pb.h"
#include "node-executor.pb.h"

#endif // USE_GRPC

#include "congestion-reporter.hpp"

namespace iceflow {

/**
 * Allows for publishing data to `IceflowConsumer`s.
 *
 * The data is split across one or more `topicPartitions` whose indexes can be
 * passed as a constructor argument.
 */
class IceflowProducer {
public:
  IceflowProducer(std::shared_ptr<ndn::svs::SVSPubSub> svsPubSub,
                  const std::string &nodePrefix, const std::string &syncPrefix,
                  const std::string &upstreamEdgeName,
                  uint32_t numberOfPartitions);

  IceflowProducer(std::shared_ptr<ndn::svs::SVSPubSub> svsPubSub,
                  const std::string &nodePrefix, const std::string &syncPrefix,
                  const std::string &upstreamEdgeName,
                  uint32_t numberOfPartitions,
                  std::shared_ptr<CongestionReporter> congestionReporter);

  ~IceflowProducer();

  void pushData(const std::vector<uint8_t> &data);

  void setTopicPartitions(uint64_t numberOfPartitions);

  uint32_t getProductionStats();

private:
  IceflowProducer(
      std::shared_ptr<ndn::svs::SVSPubSub> svsPubSub,
      const std::string &nodePrefix, const std::string &syncPrefix,
      const std::string &upstreamEdgeName, uint32_t numberOfPartitions,
      std::optional<std::shared_ptr<CongestionReporter>> congestionReporter);

  uint32_t getNextPartitionNumber();

  void reportCongestion(CongestionReason congestionReason,
                        const std::string &edgeName);

  void saveTimestamp(std::chrono::steady_clock::time_point timestamp);

  void cleanUpTimestamps(
      std::chrono::time_point<std::chrono::steady_clock> referenceTimepoint);

  ndn::Name prepareDataName(const std::string &topic, uint32_t partitionNumber);

private:
  const std::weak_ptr<ndn::svs::SVSPubSub> m_svsPubSub;
  const std::string m_pubTopic;

  std::string m_nodePrefix;

  uint32_t m_numberOfPartitions;
  std::unordered_set<uint32_t> m_topicPartitions;

  std::mt19937 m_randomNumberGenerator;

  std::optional<std::shared_ptr<CongestionReporter>> m_congestionReporter;

  std::deque<std::chrono::time_point<std::chrono::steady_clock>>
      m_productionTimestamps;

  // TODO: Make configurable
  std::chrono::seconds m_maxProductionTimestampAge = std::chrono::seconds(1);
};
} // namespace iceflow

#endif // ICEFLOW_PRODUCER_HPP
