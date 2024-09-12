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

struct QueueEntry {
  std::string topic;
  uint32_t partitionNumber;
  std::vector<uint8_t> data;
};

struct ProducerRegistrationInfo {
  std::function<QueueEntry(void)> popQueueValue;
  std::function<bool(void)> hasQueueValue;
};

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
   * Generates a new IceFlow object from a `syncPrefix`, a `nodePrefix`, and a
   * custom `face`.
   */
  IceFlow(DAGParser dagParser, const std::string &nodeName, ndn::Face &face);

public:
  void run();

  void shutdown();

  const std::string &getNodePrefix();

  const std::string &getSyncPrefix();

  void pushData(const std::string &producerEdgeName,
                std::vector<uint8_t> payload);

private:
  void unsubscribe(const std::string &consumerEdgeName);

  void
  onMissingData(const std::vector<ndn::svs::MissingDataInfo> &missing_data);

private:
  ndn::KeyChain m_keyChain;
  ndn::Face &m_face;

  std::mutex m_producerRegistrationMutex;
  std::condition_variable m_producerRegistrationConditionVariable;
  bool m_producersAvailable = false;

  bool m_running = false;

  std::shared_ptr<ndn::svs::SVSPubSub> m_svsPubSub;

  std::string m_nodePrefix;
  std::string m_syncPrefix;

  std::unordered_map<std::string, IceflowProducer> m_iceflowProducers;
  std::unordered_map<std::string, IceflowConsumer> m_iceflowConsumers;

  uint32_t m_nextProducerId = 0;
};

} // namespace iceflow

#endif // ICEFLOW_HPP
