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

#include "node-executor-service.hpp"
#include "node-instance-service.hpp"
#include "scaler.hpp"

#include <grpc++/grpc++.h>

namespace iceflow {

NDN_LOG_INIT(iceflow.IceflowScaler);

IceflowScaler::IceflowScaler(const std::string &serverAddress,
                             const std::string &clientAddress,
                             std::shared_ptr<IceFlow> iceflow)
    : m_serverAddress(serverAddress), m_clientAddress(clientAddress),
      m_iceflow(iceflow) {
  runGrpcServer(m_serverAddress);
  runGrpcClient(m_clientAddress);
};

IceflowScaler::~IceflowScaler() {
  if (m_server) {
    m_server->Shutdown();
  }
}

void IceflowScaler::runGrpcServer(const std::string &address) {
  NodeInstanceService service(m_iceflow);

  grpc::ServerBuilder builder;
  builder.AddListeningPort(address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  m_server = builder.BuildAndStart();
  NDN_LOG_INFO("Server listening on " << address);
}

void IceflowScaler::runGrpcClient(const std::string &address) {
  auto channel =
      grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
  m_nodeExecutorService = NodeExecutor::NewStub(channel, grpc::StubOptions());
}

void IceflowScaler::reportCongestion(CongestionReason congestionReason,
                                     const std::string &edgeName) {
  if (!m_nodeExecutorService) {
    NDN_LOG_WARN("NodeExecutorService instance is not available!");
  }

  CongestionReportRequest request;
  request.set_congestion_reason(congestionReason);
  request.set_edge_name(edgeName);
  CongestionReportResponse response;
  grpc::ClientContext context;

  auto status =
      m_nodeExecutorService->ReportCongestion(&context, request, &response);

  if (status.ok()) {
    NDN_LOG_INFO("Received a success reponse.");
    return;
  }

  NDN_LOG_INFO("Received an error reponse.");
}

} // namespace iceflow
