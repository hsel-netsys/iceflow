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
    std::unordered_map<std::string, std::shared_ptr<IceflowConsumer>>
        &consumerMap,
    std::unordered_map<std::string, std::shared_ptr<IceflowProducer>>
        &producerMap)
    : m_consumerMap(consumerMap), m_producerMap(producerMap){};

grpc::Status NodeInstanceService::Repartition(grpc::ServerContext *context,
                                              const RepartitionRequest *request,
                                              RepartitionResponse *response) {
  auto edgeName = request->edge_name();

  if (!m_consumerMap.contains(edgeName)) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND,
                        "No consumer defined for edge.");
  }

  auto consumer = m_consumerMap[edgeName];

  auto lowerPartitionBound = request->lower_partition_bound();
  auto upperPartitionBound = request->upper_partition_bound();

  std::cout << "Server: Repartition for NodeInstance with new partition range: "
            << lowerPartitionBound << "--" << upperPartitionBound << "."
            << std::endl;

  auto partitions =
      std::vector<uint32_t>(lowerPartitionBound, upperPartitionBound);

  auto isSuccess = consumer->repartition(partitions);

  response->set_success(isSuccess);
  response->set_lower_partition_bound(lowerPartitionBound);
  response->set_upper_partition_bound(upperPartitionBound);

  return grpc::Status::OK;
}

grpc::Status NodeInstanceService::QueryStats(grpc::ServerContext *context,
                                             const StatsRequest *request,
                                             StatsResponse *response) {

  for (auto consumerMapEntry : m_consumerMap) {
    auto consumptionStats = response->add_consumption_stats();
    consumptionStats->set_edge_name(consumerMapEntry.first);
    consumptionStats->set_units_consumed(
        consumerMapEntry.second->getConsumptionStats());
  }

  for (auto producerMapEntry : m_producerMap) {
    auto productionStats = response->add_production_stats();
    productionStats->set_edge_name(producerMapEntry.first);
    productionStats->set_units_produced(
        producerMapEntry.second->getProductionStats());
  }

  return grpc::Status::OK;
}
} // namespace iceflow
