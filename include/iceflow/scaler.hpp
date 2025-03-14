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

#ifndef ICEFLOW_SCALER_HPP
#define ICEFLOW_SCALER_HPP

#include "congestion-reporter.hpp"
#include "node-instance-service.hpp"

#include "iceflow.hpp"
#include "producer.hpp"

namespace iceflow {

class IceflowScaler : public CongestionReporter {
public:
  IceflowScaler(const std::string &serverAddress,
                const std::string &clientAddress,
                std::shared_ptr<IceFlow> iceflow);

  ~IceflowScaler() override;

  void reportCongestion(CongestionReason congestionReason,
                        const std::string &edgeName) override;

  void registerConsumer(const std::string &edgeName,
                        std::shared_ptr<IceflowConsumer> consumer);

  void deregisterConsumer(const std::string &edgeName);

  void registerProducer(const std::string &edgeName,
                        std::shared_ptr<IceflowProducer> producer);

  void deregisterProducer(const std::string &edgeName);

private:
  void runGrpcServer(const std::string &address);

  void runGrpcClient(const std::string &address);

private:
  const std::string &m_serverAddress;

  const std::string &m_clientAddress;

  std::shared_ptr<IceFlow> m_iceflow;

  std::shared_ptr<iceflow::NodeInstanceService> m_instanceService;

  std::unique_ptr<grpc::Server> m_server;

  std::unique_ptr<NodeExecutor::Stub> m_nodeExecutorService;

  std::unordered_map<std::string, std::shared_ptr<IceflowConsumer>>
      m_consumerMap;

  std::unordered_map<std::string, std::shared_ptr<IceflowProducer>>
      m_producerMap;
};
} // namespace iceflow

#endif // ICEFLOW_SCALER_HPP
