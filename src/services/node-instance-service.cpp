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

namespace iceflow {

NDN_LOG_INIT(iceflow.services.NodeInstanceService);

NodeInstanceService::NodeInstanceService(std::shared_ptr<IceFlow> iceflow)
    : m_iceflow(iceflow){};

grpc::Status NodeInstanceService::Repartition(grpc::ServerContext *context,
                                              const RepartitionRequest *request,
                                              RepartitionResponse *response) {
  auto edgeName = request->edge_name();

  auto lowerPartitionBound = request->lower_partition_bound();
  auto upperPartitionBound = request->upper_partition_bound();

  NDN_LOG_INFO("Server: Repartition for edge "
               << edgeName << " of NodeInstance with new partition range : "
               << lowerPartitionBound << "--" << upperPartitionBound << ".");

  auto partitions =
      std::vector<uint32_t>(lowerPartitionBound, upperPartitionBound);

  response->set_lower_partition_bound(lowerPartitionBound);
  response->set_upper_partition_bound(upperPartitionBound);

  try {
    m_iceflow->repartitionConsumer(edgeName, partitions);

    // TODO: Maybe this field is actually obsolete
    response->set_success(true);

    return grpc::Status::OK;
  } catch (...) {
    // TODO: Improve error handling
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "Consumer repartitioning has failed.");
  }
}

grpc::Status NodeInstanceService::QueryStats(grpc::ServerContext *context,
                                             const StatsRequest *request,
                                             StatsResponse *response) {

  auto consumerStats = m_iceflow->getConsumerStats();
  auto producerStats = m_iceflow->getProducerStats();

  for (auto consumerStat : consumerStats) {
    auto consumptionStats = response->add_consumption_stats();
    consumptionStats->set_edge_name(consumerStat.first);
    consumptionStats->set_units_consumed(consumerStat.second.unitsConsumed);
    consumptionStats->set_idle_time(consumerStat.second.idleTime);
  }

  for (auto producerStat : producerStats) {
    auto productionStats = response->add_production_stats();
    productionStats->set_edge_name(producerStat.first);
    productionStats->set_units_produced(producerStat.second.unitsProduced);
    productionStats->set_idle_time(producerStat.second.idleTime);
  }

  return grpc::Status::OK;
}
} // namespace iceflow
