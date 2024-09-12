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
  auto upstreamEdges = dagParser.findUpstreamEdges(node.task);

  ndn::svs::SecurityOptions secOpts(m_keyChain);

  ndn::svs::SVSPubSubOptions opts;

  m_svsPubSub = std::make_shared<ndn::svs::SVSPubSub>(
      ndn::Name(m_syncPrefix), ndn::Name(m_nodePrefix), m_face,
      std::bind(&IceFlow::onMissingData, this, _1), opts, secOpts);

  for (auto downstreamEdge : downstreamEdges) {
    std::cout << downstreamEdge.id << std::endl;
  }

  for (auto upstreamEdge : upstreamEdges) {
    std::cout << upstreamEdge.second.id << std::endl;
  }
};

void IceFlow::run() {
  if (m_running) {
    throw std::runtime_error("Iceflow instance is already running!");
  }

  std::thread svsThread([this] {
    while (true) {
      try {
        m_face.processEvents(ndn::time::milliseconds(5000), true);
      } catch (std::exception &e) {
        NDN_LOG_ERROR("Error in event handling loop: " << e.what());
      }
    }
  });

  svsThread.join();
}

void IceFlow::shutdown() { m_running = false; }

// uint32_t IceFlow::subscribeToTopicPartition(
//     const std::string &topic, uint32_t partitionNumber,
//     std::function<void(std::vector<uint8_t>)> &pushDataCallback) {

//   auto subscribedTopic = ndn::Name(topic).appendNumber(partitionNumber);

//   auto subscriptionHandle = m_svsPubSub->subscribe(
//       subscribedTopic, std::bind(&IceFlow::subscribeCallBack, this,
//                                  pushDataCallback, std::placeholders::_1));

//   NDN_LOG_INFO("Subscribed to " << subscribedTopic);

//   return subscriptionHandle;
// }

void IceFlow::unsubscribe(const std::string &consumerEdgeName) {
  auto consumer = m_iceflowConsumers.at(consumerEdgeName);
  // TODO: Do something here.
}

// void IceFlow::subscribeCallBack(
//     const std::function<void(std::vector<uint8_t>)> &pushDataCallback,
//     const ndn::svs::SVSPubSub::SubscriptionData &subData) {
//   NDN_LOG_DEBUG("Producer Prefix: " << subData.producerPrefix << " ["
//                                     << subData.seqNo << "] : " <<
//                                     subData.name
//                                     << " : ");

//   std::vector<uint8_t> data(subData.data.begin(), subData.data.end());

//   pushDataCallback(data);
// }

void IceFlow::onMissingData(
    const std::vector<ndn::svs::MissingDataInfo> &missing_data) {
  // TODO: Implement if needed
}

void IceFlow::pushData(const std::string &producerEdgeName,
                       std::vector<uint8_t> payload) {
  auto producer = m_iceflowProducers.at(producerEdgeName);
  producer.pushData(payload);
}

const std::string &IceFlow::getNodePrefix() { return m_nodePrefix; }
const std::string &IceFlow::getSyncPrefix() { return m_syncPrefix; }

} // namespace iceflow
