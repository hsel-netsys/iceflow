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

#ifndef DAGPARSER_HPP
#define DAGPARSER_HPP

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace iceflow {

// Structs for resources, container, scaling parameters, and communication task
struct Resources {
  uint32_t cpu;
  uint32_t memory;
};

struct Container {
  std::string image;
  Resources resources;
};

struct ScalingParameters {
  uint32_t taskComplexity;
};

struct CommunicationTask {
  std::optional<uint32_t> payloadSize;
};

struct Edge {
  std::string id;
  std::string target;
  uint32_t maxPartitions;
};

/**
 * Parameters of a node in the DAG.
 */
struct Node {
  std::string task;
  std::string name;
  std::string description;
  std::string executor;
  Container container;
  ScalingParameters scalingParameters;
  CommunicationTask communicationTask;
  std::vector<Edge> downstream;
  nlohmann::json::object_t applicationConfiguration;
};

/**
 * Class for parsing the application DAG.
 */
class DAGParser {
public:
  DAGParser(const std::string &appName, const std::vector<Node> &nodes);

  static DAGParser parseFromFile(const std::string &filename);

  const std::vector<Node> &getNodes();

  const Node &findNodeByName(const std::string &nodeName);

  const Edge &findEdgeByName(const std::string &edgeId);

  std::vector<std::pair<const Node &, const Edge &>>
  findUpstreamEdges(const std::string &node_name);

private:
  std::string applicationName;
  std::vector<Node> nodes;
};

} // namespace iceflow

#endif // DAGPARSER_HPP
