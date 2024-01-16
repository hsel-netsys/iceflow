/*
 * Copyright 2021 The IceFlow Authors.
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

#include "constants.hpp"
#include "data.hpp"
#include "logger.hpp"
#include "ndn-svs/security-options.hpp"
#include "ringbuffer.hpp"
#include <ndn-svs/svspubsub.hpp>

#include <iostream>
#include <thread>

namespace iceflow {

class IceFlowPub {

public:
  IceFlowPub(const std::string &syncPrefix, const std::string &topic,
             const std::vector<int> &nTopic, ndn::Face &interFace)
      : ndnInterFace(interFace), m_scheduler(ndnInterFace.getIoContext()),
        pubTopic(topic), topicPartition(nTopic) {

    ndn::svs::SecurityOptions secOpts(m_keyChain);

    // Do not fetch publications older than 10 seconds
    ndn::svs::SVSPubSubOptions opts;
    //    opts.maxPubAge = ndn::time::milliseconds(10000);

    m_SvSPro = std::make_shared<ndn::svs::SVSPubSub>(
        ndn::Name(syncPrefix), ndn::Name(pubTopic), ndnInterFace,
        std::bind(&IceFlowPub::fetchingStateVector, this, _1), opts, secOpts);
  }

  virtual ~IceFlowPub() = default;

public:
  RingBuffer<std::string> outputQueue;
  RingBuffer<std::string> inputQueue;
  ndn::Face &ndnInterFace;

  void subscribe(std::string subscribeTopic) {
    // Allows consumer to select a particular topic partition
    auto subscribed = m_SvSPro->subscribe(
        subscribeTopic,
        std::bind(&IceFlowPub::subscribeCallBack, this, std::placeholders::_1));
    std::cout << "Subscribed to: " << subscribed << std::endl;
  }

  void subscribeCallBack(const ndn::svs::SVSPubSub::SubscriptionData &subData) {
    //	  for (int i= 0; i<subData.data.size();i++){
    //		  std::cout<<subData.data[i] << std::endl;
    //	  }
    std::string content(reinterpret_cast<const char *>(subData.data.data()),
                        subData.data.size());
    std::cout << subData.producerPrefix << " [" << subData.seqNo
              << "] : " << subData.name << " : ";
    if (content.length() > 200) {
      std::cout << "[LONG] " << content.length() << " bytes"
                << " [" << std::hash<std::string>{}(content) << "]";
    } else {
      std::cout << content;
    }
    std::cout << std::endl;
    inputQueue.push(content);
  }

  void fetchingStateVector(
      const std::vector<ndn::svs::MissingDataInfo> &missing_data) {
    // Iterate over the entire difference set
    //    for (const auto &info : missing_data) {
    //      // Iterate over each new sequence number that we learned
    //      for (ndn::svs::SeqNo sequence = info.low; sequence <= info.high;
    //           ++sequence) {
    //        // Process the missing data info here if needed
    //      }
    //    }
  }

  ndn::Name prepareDataName() {
    if (partitionCount > topicPartition.size()) {
      partitionCount = 1;
    }
    dataName = pubTopic + "/" + std::to_string(partitionCount);
    partitionCount++;
    dataCount++;
    return dataName;
  }

  void publishMsg() {
    if (!outputQueue.empty()) {
      auto dataID = prepareDataName();
      auto payload = outputQueue.waitAndPopValue();
      std::cout << "Payload: " << payload << std::endl;
      auto encodedContent = ndn::make_span(
          reinterpret_cast<const uint8_t *>(payload.data()), payload.size());
      auto sequenceNo = m_SvSPro->publish(pubTopic, encodedContent, ndn::Name(),
                                          ndn::time::milliseconds(10));
      std::cout << "Publish: " << pubTopic << "/" << sequenceNo << std::endl;
    }
    m_scheduler.schedule(ndn::time::milliseconds(1), [this] { publishMsg(); });
  }

  void storeFromQueue(ndn::Name &namedData, ndn::Block &data) {
    dataStorage.insert(std::pair(namedData, data));
  }

  void onInterest(const ndn::InterestFilter &, const ndn::Interest &interest) {
    NDN_LOG_DEBUG(">> Got Interest: " << interest.getName());
    auto var = dataStorage.find(interest.getName().toUri());
    if (var != dataStorage.end()) {
      ndn::Block mainDataBlock = dataStorage[interest.getName().toUri()];
      auto dataPacket =
          Data(interest.getName(), mainDataBlock, mainDataBlock.type());
      dataPacket.setFreshnessPeriod(constants::PRODUCER_FRESHNESS_PERIOD);
      m_keyChain.sign(dataPacket);
      ndnInterFace.put(dataPacket);
    }
  }

  void onRegisterFailed(const ndn::Name &prefix, const std::string &reason) {
    NDN_LOG_ERROR("ERROR: Failed to register prefix '"
                  << prefix << "' with the local forwarder (" << reason << ")");
    ndnInterFace.shutdown();
  }

private:
  std::shared_ptr<ndn::svs::SVSPubSub> m_SvSPro;
  ndn::Scheduler m_scheduler;
  ndn::KeyChain m_keyChain;
  const ndn::svs::UpdateCallback m_onUpdate;
  int partitionCount = 1;
  const std::string pubTopic;
  const std::vector<int> topicPartition;
  ndn::Name dataName;
  int dataCount = 1;
  std::map<ndn::Name, ndn::Block> dataStorage;
};

} // namespace iceflow
