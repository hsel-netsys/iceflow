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

#include "node-instance-service.hpp"
#include "scaler.hpp"

namespace iceflow {

NDN_LOG_INIT(iceflow.IceflowScaler);

IceflowScaler::IceflowScaler(std::shared_ptr<IceflowConsumer> consumer,
                             const std::string &serverAddress)
    : m_consumer(consumer), m_serverAddress(serverAddress) {
  runGrpcServer(m_serverAddress);
};

IceflowScaler::~IceflowScaler() {
  if (m_server) {
    m_server->Shutdown();
  }
}

void IceflowScaler::runGrpcServer(const std::string &address) {
  NodeInstanceService service(m_consumer);

  grpc::ServerBuilder builder;
  builder.AddListeningPort(address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  m_server = builder.BuildAndStart();
  NDN_LOG_INFO("Server listening on " << address);
}
} // namespace iceflow
