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

#include "node-instance-service.hpp"

namespace iceflow {

NodeInstanceService::NodeInstanceService(
    std::weak_ptr<IceflowConsumer> consumer)
    : m_consumer(consumer){};

grpc::Status NodeInstanceService::Repartition(grpc::ServerContext *context,
                                              const RepartitionRequest *request,
                                              RepartitionResponse *response) {
  if (auto validConsumer = m_consumer.lock()) {

    auto lowerPartitionBound = request->lower_partition_bound();
    auto upperPartitionBound = request->upper_partition_bound();

    std::cout
        << "Server: Repartition for NodeInstance with new partition range: "
        << lowerPartitionBound << "--" << upperPartitionBound << "."
        << std::endl;

    auto partitions =
        std::vector<uint32_t>(lowerPartitionBound, upperPartitionBound);

    auto isSuccess = validConsumer->repartition(partitions);

    response->set_success(isSuccess);
    response->set_lower_partition_bound(lowerPartitionBound);
    response->set_upper_partition_bound(upperPartitionBound);

    return grpc::Status::OK;
  }

  return grpc::Status(grpc::StatusCode::INTERNAL,
                      "NodeInstanceService has already been shut down");
}
} // namespace iceflow
