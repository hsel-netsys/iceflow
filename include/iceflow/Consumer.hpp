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

#ifndef ICEFLOW_Consumer_HPP
#define ICEFLOW_Consumer_HPP
#include "IceFlowPubBase.hpp"
#include <string>

namespace iceflow {
class Consumer {
public:
  Consumer(const std::string &syncPrefix, const std::string &topic,
           const std::vector<int> &nTopic, ndn::Face &interFace)
      : subscribedTopic(topic), ConsumerFace(interFace),
        baseConsumer(syncPrefix, topic, nTopic, interFace) {}

  std::string receive() {
    auto receivedData = baseConsumer.inputQueue.waitAndPopValue();
    return receivedData;
  }
  void run() {
    std::vector<std::thread> processing_threads;
    baseConsumer.subscribe(subscribedTopic);
    processing_threads.emplace_back([this] { ConsumerFace.processEvents(); });
    int threadCounter = 0;
    for (auto &thread : processing_threads) {
      std::cout << "Thread " << threadCounter++ << " started" << std::endl;
      thread.join();
    }
  }

private:
  iceflow::IceFlowPub baseConsumer;
  std::string subscribedTopic;
  ndn::Face &ConsumerFace;
};
} // namespace iceflow
#endif // ICEFLOW_CONSUMER_HPP