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

#include <random>
#include <unordered_set>

#include <time.h>

#include <iceflow/node-executor.grpc.pb.h>
#include <iceflow/node-executor.pb.h>

#include "iceflow.hpp"

namespace iceflow {

class CongestionReporter {
public:
  virtual ~CongestionReporter() {}
  virtual void reportCongestion(CongestionReason congestionReason,
                                const std::string &edgeName) = 0;
};

/**
 * Allows for publishing data to `IceflowConsumer`s.
 *
 * The data is split across one or more `topicPartitions` whose indexes can be
 * passed as a constructor argument.
 */
class IceflowProducer {
public:
  IceflowProducer(std::shared_ptr<IceFlow> iceflow, const std::string &pubTopic,
                  uint32_t numberOfPartitions,
                  std::chrono::milliseconds publishInterval);

  IceflowProducer(std::shared_ptr<IceFlow> iceflow, const std::string &pubTopic,
                  uint32_t numberOfPartitions,
                  std::chrono::milliseconds publishInterval,
                  std::shared_ptr<CongestionReporter> congestionReporter);

  ~IceflowProducer();

  void pushData(const std::vector<uint8_t> &data);

  void setPublishInterval(std::chrono::milliseconds publishInterval);

  void setTopicPartitions(uint64_t numberOfPartitions);

private:
  IceflowProducer(
      std::shared_ptr<IceFlow> iceflow, const std::string &pubTopic,
      uint32_t numberOfPartitions, std::chrono::milliseconds publishInterval,
      std::optional<std::shared_ptr<CongestionReporter>> congestionReporter);

  uint32_t getNextPartitionNumber();

  QueueEntry popQueueValue();

  bool hasQueueValue();

  void resetLastPublishTimePoint();

  std::chrono::time_point<std::chrono::steady_clock> getNextPublishTimePoint();

  void reportCongestion(CongestionReason congestionReason,
                        const std::string &edgeName);

private:
  const std::weak_ptr<IceFlow> m_iceflow;
  const std::string m_pubTopic;
  RingBuffer<std::vector<uint8_t>> m_outputQueue;

  uint32_t m_numberOfPartitions;
  std::unordered_set<uint32_t> m_topicPartitions;

  std::chrono::nanoseconds m_publishInterval;
  std::chrono::time_point<std::chrono::steady_clock> m_lastPublishTimePoint;

  uint64_t m_subscriberId;

  std::mt19937 m_randomNumberGenerator;

  std::optional<std::shared_ptr<CongestionReporter>> m_congestionReporter;
};
} // namespace iceflow

#endif // ICEFLOW_PRODUCER_HPP
