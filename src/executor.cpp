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

#include "ndn-cxx/util/logger.hpp"

#include "executor.hpp"
#include "node-executor-service.hpp"
#include "node-instance-service.hpp"

#include <grpc++/grpc++.h>

namespace iceflow {

NDN_LOG_INIT(iceflow.IceflowExecutor);

IceflowExecutor::IceflowExecutor(
    const std::string &serverAddress, const std::string &clientAddress,
    std::shared_ptr<ExternalExecutor> externalExecutor)
    : m_serverAddress(serverAddress), m_clientAddress(clientAddress),
      m_externalExecutor(externalExecutor) {
  runGrpcServer(m_serverAddress);
  runGrpcClient(m_clientAddress);
};

IceflowExecutor::~IceflowExecutor() {
  if (m_server) {
    m_server->Shutdown();
  }
}

void IceflowExecutor::runGrpcServer(const std::string &address) {
  auto executorPointer = weak_from_this();
  auto service = NodeExecutorService(executorPointer);

  grpc::ServerBuilder builder;
  builder.AddListeningPort(address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  m_server = builder.BuildAndStart();
  NDN_LOG_INFO("Server listening on " << address);
}

void IceflowExecutor::runGrpcClient(const std::string &address) {
  auto channel =
      grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
  m_nodeInstanceService = NodeInstance::NewStub(channel, grpc::StubOptions());
}

void IceflowExecutor::repartition(uint32_t lowerPartitionBound,
                                  uint32_t upperPartitionBound) {
  if (!m_nodeInstanceService) {
    NDN_LOG_WARN("NodeInstanceService instance is not available!");
    return;
  }

  RepartitionRequest request;
  request.set_lower_partition_bound(lowerPartitionBound);
  request.set_upper_partition_bound(upperPartitionBound);

  RepartitionResponse response;
  grpc::ClientContext context;

  auto status =
      m_nodeInstanceService->Repartition(&context, request, &response);

  if (status.ok()) {
    NDN_LOG_INFO("Received a success response.");
    return;
  }

  NDN_LOG_INFO("Received an error response.");
}

void IceflowExecutor::receiveCongestionReport(CongestionReason congestionReason,
                                              const std::string &edge_name) {
  m_externalExecutor->receiveCongestionReport(congestionReason, edge_name);
}

} // namespace iceflow
