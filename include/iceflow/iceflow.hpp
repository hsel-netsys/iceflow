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

#ifndef ICEFLOW_HPP
#define ICEFLOW_HPP

#include "congestion-reporter.hpp"
#include "consumer.hpp"
#include "dag-parser.hpp"
#include "producer.hpp"

#include "ndn-svs/security-options.hpp"
#include "ndn-svs/svspubsub.hpp"

#include <chrono>
#include <iostream>
#include <thread>
#include <unordered_map>

namespace iceflow {

typedef std::function<void(const std::string &, std::vector<uint8_t>)>
    ProducerCallback;
typedef std::function<void(std::vector<uint8_t>, ProducerCallback)>
    ProsumerCallback;

/**
 * Central building block for IceFlow-based consumers and producers.
 *
 * Will act as a consumer if a `subTopic` is defined and act as a consumer if
 * a `pubTopic` is defined.
 * If both topics are passed to the constructor, the instantiated IceFlow
 * object will act both as a producer _and_ a consumer.
 */
class IceFlow {
public:
  /**
   * Generates a new IceFlow object from a `dagParser`, a `nodeName`, and a
   * custom `face`.
   */
  IceFlow(DAGParser dagParser, const std::string &nodeName, ndn::Face &face);

  IceFlow(DAGParser dagParser, const std::string &nodeName, ndn::Face &face,
          std::shared_ptr<CongestionReporter> congestionReporter);

  IceFlow(const std::string &dagFileName, const std::string &nodeName,
          ndn::Face &face);

  IceFlow(const std::string &dagFileName, const std::string &nodeName,
          ndn::Face &face,
          std::shared_ptr<CongestionReporter> congestionReporter);

private:
  IceFlow(
      DAGParser dagParser, const std::string &nodeName, ndn::Face &face,
      std::optional<std::shared_ptr<CongestionReporter>> congestionReporter);

  IceFlow(
      const std::string &dagFileName, const std::string &nodeName,
      ndn::Face &face,
      std::optional<std::shared_ptr<CongestionReporter>> congestionReporter);

public:
  void run();

  void shutdown();

  const std::string &getNodePrefix();

  const std::string &getSyncPrefix();

  void pushData(const std::string &downstreamEdgeName,
                std::vector<uint8_t> payload);

  void registerConsumerCallback(const std::string &upstreamEdgeName,
                                ConsumerCallback consumerCallback);

  void registerProsumerCallback(const std::string &upstreamEdgeName,
                                ProsumerCallback prosumerCallback);

  void repartitionConsumer(const std::string &upstreamEdgeName,
                           std::vector<uint32_t> partitions);

  void repartitionProducer(const std::string &downstreamEdgeName,
                           uint64_t numberOfPartitions);

  std::unordered_map<std::string, EdgeConsumptionStats> getConsumerStats();

  std::unordered_map<std::string, EdgeProductionStats> getProducerStats();

  void reportCongestion(const std::string &edgeName,
                        CongestionReason congestionReason);

  std::vector<Edge> getDownstreamEdges();

  std::optional<Edge> getDownstreamEdge(uint32_t index);

  std::vector<Edge> getUpstreamEdges();

  std::optional<Edge> getUpstreamEdge(uint32_t index);

  nlohmann::json::object_t getApplicationConfiguration() {
    return m_node.applicationConfiguration;
  }

  template <typename T>
  std::optional<T> getApplicationParameter(const std::string &key) {
    auto applicationConfiguration = getApplicationConfiguration();

    if (!applicationConfiguration.contains(key)) {
      return std::nullopt;
    }

    try {
      auto value = applicationConfiguration.at(key).get<T>();
      return std::optional(value);
    } catch (...) {
      return std::nullopt;
    }
  }

private:
  void
  onMissingData(const std::vector<ndn::svs::MissingDataInfo> &missing_data);

private:
  ndn::KeyChain m_keyChain;
  ndn::Face &m_face;

  bool m_running = false;

  std::shared_ptr<ndn::svs::SVSPubSub> m_svsPubSub;

  std::string m_nodePrefix;
  std::string m_syncPrefix;

  std::unordered_map<std::string, IceflowProducer> m_iceflowProducers;

  std::unordered_map<std::string, IceflowConsumer> m_iceflowConsumers;

  std::optional<std::shared_ptr<CongestionReporter>> m_congestionReporter;

  Node m_node;

  std::vector<Edge> m_downstreamEdges;

  std::vector<Edge> m_upstreamEdges;
};

} // namespace iceflow

#endif // ICEFLOW_HPP
