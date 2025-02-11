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

#include "node-executor-service.hpp"

namespace iceflow {

NodeExecutorService::NodeExecutorService(
    std::weak_ptr<IceflowExecutor> executor)
    : m_executor(executor){};

grpc::Status
NodeExecutorService::ReportCongestion(grpc::ServerContext *context,
                                      const CongestionReportRequest *request,
                                      CongestionReportResponse *response) {
  if (auto validExecutor = m_executor.lock()) {

    auto edgeName = request->edge_name();
    auto congestionReason = request->congestion_reason();

    validExecutor->receiveCongestionReport(congestionReason, edgeName);

    return grpc::Status::OK;
  }

  return grpc::Status(grpc::StatusCode::INTERNAL,
                      "NodeExecutorService has already been shut down");
}
} // namespace iceflow
