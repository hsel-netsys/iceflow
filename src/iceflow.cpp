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

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "ndn-cxx/util/logger.hpp"

#include "iceflow.hpp"

namespace iceflow {

std::string generateNodePrefix() {
  boost::uuids::basic_random_generator<boost::mt19937> gen;
  boost::uuids::uuid uuid = gen();
  return to_string(uuid);
}

NDN_LOG_INIT(iceflow.IceFlow);

IceFlow::IceFlow(DAGParser dagParser, const std::string &nodeName,
                 ndn::Face &face)
    : m_face(face) {
  m_nodePrefix = generateNodePrefix();
  m_syncPrefix = "/" + dagParser.getApplicationName();

  auto node = dagParser.findNodeByName(nodeName);
  auto downstreamEdges = node.downstream;
  auto upstreamEdges = dagParser.findUpstreamEdges(node);

  ndn::svs::SecurityOptions secOpts(m_keyChain);

  ndn::svs::SVSPubSubOptions opts;

  m_svsPubSub = std::make_shared<ndn::svs::SVSPubSub>(
      ndn::Name(m_syncPrefix), ndn::Name(m_nodePrefix), m_face,
      std::bind(&IceFlow::onMissingData, this, _1), opts, secOpts);

  for (auto downstreamEdge : downstreamEdges) {
    auto downstreamEdgeName = downstreamEdge.id;

    auto iceflowProducer =
        IceflowProducer(m_svsPubSub, m_nodePrefix, m_syncPrefix,
                        downstreamEdgeName, downstreamEdge.maxPartitions);

    m_iceflowProducers.emplace(downstreamEdgeName, iceflowProducer);
  }

  for (auto upstreamEdge : upstreamEdges) {
    auto upstreamEdgeName = upstreamEdge.second.id;

    auto iceflowConsumer =
        IceflowConsumer(m_svsPubSub, m_syncPrefix, upstreamEdgeName);

    m_iceflowConsumers.emplace(upstreamEdgeName, iceflowConsumer);
  }
};

void IceFlow::run() {
  if (m_running) {
    throw std::runtime_error("Iceflow instance is already running!");
  }

  m_running = true;

  std::thread svsThread([this] {
    while (m_running) {
      try {
        m_face.processEvents(ndn::time::milliseconds(5000), true);
      } catch (std::exception &e) {
        NDN_LOG_ERROR("Error in event handling loop: " << e.what());
      }
    }
  });

  svsThread.join();
}

void IceFlow::repartitionConsumer(const std::string &upstreamEdgeName,
                                  std::vector<uint32_t> partitions) {
  if (!m_iceflowConsumers.contains(upstreamEdgeName)) {
    throw std::runtime_error("Consumer for upstream edge " + upstreamEdgeName +
                             " does not exist!");
  }

  m_iceflowConsumers.at(upstreamEdgeName).repartition(partitions);
}

void IceFlow::repartitionProducer(const std::string &downstreamEdgeName,
                                  uint64_t numberOfPartitions) {
  if (!m_iceflowProducers.contains(downstreamEdgeName)) {
    throw std::runtime_error("Producer for downstream edge " +
                             downstreamEdgeName + " does not exist!");
  }

  m_iceflowProducers.at(downstreamEdgeName)
      .setTopicPartitions(numberOfPartitions);
}

void IceFlow::shutdown() { m_running = false; }

void IceFlow::onMissingData(
    const std::vector<ndn::svs::MissingDataInfo> &missing_data) {
  // TODO: Implement if needed
}

void IceFlow::pushData(const std::string &downstreamEdgeName,
                       std::vector<uint8_t> payload) {

  if (!m_iceflowProducers.contains(downstreamEdgeName)) {
    throw std::runtime_error("Producer for downstream edge " +
                             downstreamEdgeName + " does not exist!");
  }

  m_iceflowProducers.at(downstreamEdgeName).pushData(payload);
}

void IceFlow::registerConsumerCallback(const std::string &upstreamEdgeName,
                                       ConsumerCallback consumerCallback) {
  if (!m_iceflowConsumers.contains(upstreamEdgeName)) {
    throw std::runtime_error("Consumer for upstream edge " + upstreamEdgeName +
                             " does not exist!");
  }

  m_iceflowConsumers.at(upstreamEdgeName).setConsumerCallback(consumerCallback);
}

void IceFlow::registerProsumerCallback(const std::string &upstreamEdgeName,
                                       ProsumerCallback prosumerCallback) {

  auto producerCallback = [this](const std::string &downstreamEdgeName,
                                 std::vector<uint8_t> data) {
    pushData(downstreamEdgeName, data);
  };

  auto internalConsumerCallback = [this, &prosumerCallback, producerCallback](
                                      const std::vector<uint8_t> &data) {
    prosumerCallback(data, producerCallback);
  };

  registerConsumerCallback(upstreamEdgeName, internalConsumerCallback);
}

const std::string &IceFlow::getNodePrefix() { return m_nodePrefix; }

const std::string &IceFlow::getSyncPrefix() { return m_syncPrefix; }

std::unordered_map<std::string, uint32_t> IceFlow::getConsumerStats() {
  auto result = std::unordered_map<std::string, uint32_t>();

  for (auto consumer : m_iceflowConsumers) {
    result.emplace(consumer.first, consumer.second.getConsumptionStats());
  }

  return result;
}

std::unordered_map<std::string, uint32_t> IceFlow::getProducerStats() {
  auto result = std::unordered_map<std::string, uint32_t>();

  for (auto producer : m_iceflowProducers) {
    result.emplace(producer.first, producer.second.getProductionStats());
  }

  return result;
}

std::unordered_map<std::string, IceflowConsumer> m_iceflowConsumers;

std::unordered_map<std::string, IceflowProducer> m_iceflowProducers;
} // namespace iceflow
