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

#ifndef ICEFLOW_NODE_INSTANCE_SERVICE_H
#define ICEFLOW_NODE_INSTANCE_SERVICE_H

#ifdef USE_GRPC

#include "iceflow.hpp"

#include "node-instance.grpc.pb.h"
#include "node-instance.pb.h"

#include <grpc/grpc.h>
#include <grpcpp/server_builder.h>

namespace iceflow {

class NodeInstanceService final : public NodeInstance::Service {
public:
  explicit NodeInstanceService(std::shared_ptr<IceFlow> iceflow);

public:
  virtual grpc::Status Repartition(grpc::ServerContext *context,
                                   const RepartitionRequest *request,
                                   RepartitionResponse *response);

  virtual grpc::Status QueryStats(grpc::ServerContext *context,
                                  const StatsRequest *request,
                                  StatsResponse *response);

private:
  std::shared_ptr<IceFlow> m_iceflow;
};
} // namespace iceflow

#endif // USE_GRPC

#endif // ICEFLOW_NODE_INSTANCE_SERVICE_H
