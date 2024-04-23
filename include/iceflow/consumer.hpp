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

#ifndef ICEFLOW_CONSUMER_HPP
#define ICEFLOW_CONSUMER_HPP

#include "iceflow.hpp"

namespace iceflow {

/**
 * Allows for subscribing to data published by `IceflowProducer`s.
 */
class IceflowConsumer {

public:
  IceflowConsumer(std::shared_ptr<IceFlow> iceflow, const std::string &subTopic,
                  const std::vector<int> &topicPartitions)
      : m_iceflow(iceflow), m_subTopic(subTopic),
        m_topicPartitions(topicPartitions) {

    for (auto topicPartition : m_topicPartitions) {
      auto subscriptionHandle = subscribeToTopicPartition(topicPartition);
      m_subscriptionHandles.push_back(subscriptionHandle);
    }
  }

  ~IceflowConsumer() { unsubscribeFromAllPartitions(); }

  std::vector<uint8_t> receiveData() { return m_inputQueue.waitAndPopValue(); }

private:
  uint32_t subscribeToTopicPartition(int topicPartition) {
    if (auto validIceflow = m_iceflow.lock()) {

      std::function<void(std::vector<uint8_t>)> pushDataCallback =
          std::bind(&RingBuffer<std::vector<uint8_t>>::push, &m_inputQueue,
                    std::placeholders::_1);

      return validIceflow->subscribeToTopicPartition(m_subTopic, topicPartition,
                                                     pushDataCallback);
    } else {
      throw std::runtime_error("Iceflow instance has already expired.");
    }
  }

  void unsubscribeFromAllPartitions() {
    if (auto validIceflow = m_iceflow.lock()) {
      for (auto subscriptionHandle : m_subscriptionHandles) {
        validIceflow->unsubscribe(subscriptionHandle);
      }
    }
  }

private:
  const std::weak_ptr<IceFlow> m_iceflow;
  const std::string m_subTopic;

  const std::vector<int> m_topicPartitions;
  std::vector<uint32_t> m_subscriptionHandles;

  RingBuffer<std::vector<uint8_t>> m_inputQueue;
};
} // namespace iceflow

#endif // ICEFLOW_CONSUMER_HPP
