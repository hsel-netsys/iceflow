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
#ifndef ICEFLOW_Producer_HPP
#define ICEFLOW_Producer_HPP
#include "IceFlowPubBase.hpp"
#include "ndn-cxx/face.hpp"
#include <string>
namespace iceflow {
class Producer {

public:
  Producer(const std::string &syncPrefix, const std::string &topic,
           const std::vector<int> &nTopic, ndn::Face &interFace)
      : ProducerFace(interFace),

        baseProducer(syncPrefix, topic, nTopic, interFace,
                     interFace.getIoContext()) {
    std::cout << "Starting IceFlow Producer - - - -" << std::endl;
  }

  virtual ~Producer() = default;

  void push(std::string &data) { baseProducer.outputQueue.push(data); }

  void run() {
    std::vector<std::thread> processing_threads;
    processing_threads.emplace_back([this] { ProducerFace.processEvents(); });
    baseProducer.publishMsg();
    int threadCounter = 0;
    for (auto &thread : processing_threads) {
      std::cout << "Producer Thread " << threadCounter++ << " started"
                << std::endl;
      thread.join();
    }
  }

private:
  iceflow::IceFlowPub baseProducer;
  ndn::Face &ProducerFace;
};
} // namespace iceflow
#endif // ICEFLOW_PRODUCER_HPP
