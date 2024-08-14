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

#ifndef ICEFLOW_NODE_EXECUTOR_HPP
#define ICEFLOW_NODE_EXECUTOR_HPP

#include "iceflow.hpp"

#include <iceflow.grpc.pb.h>

#include <grpc/grpc.h>

namespace iceflow {

class ExternalExecutor {
public:
  virtual ~ExternalExecutor() {}
  virtual void receiveCongestionReport(CongestionReason congestionReason,
                                       const std::string &edgeName) = 0;
};

class IceflowExecutor : public std::enable_shared_from_this<IceflowExecutor> {
public:
  IceflowExecutor(const std::string &serverAddress,
                  const std::string &clientAddress,
                  std::shared_ptr<ExternalExecutor> externalExecutor);

  ~IceflowExecutor();

  void receiveCongestionReport(CongestionReason congestionReason,
                               const std::string &edgeName);

  void repartition(uint32_t lowerPartitionBound, uint32_t upperPartitionBound);

private:
  void runGrpcServer(const std::string &address);

  void runGrpcClient(const std::string &address);

private:
  const std::string &m_serverAddress;

  const std::string &m_clientAddress;

  std::unique_ptr<grpc::Server> m_server;

  std::unique_ptr<NodeInstance::Stub> m_nodeInstanceService;

  std::shared_ptr<ExternalExecutor> m_externalExecutor;
};
} // namespace iceflow

#endif // ICEFLOW_NODE_EXECUTOR_HPP