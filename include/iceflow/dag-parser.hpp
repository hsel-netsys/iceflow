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
  std::string tag;
  std::map<std::string, std::string> envs;
  std::map<std::string, std::string> mounts;
  Resources resources;
};

struct ScalingParameters {
  uint32_t taskComplexity;
};

struct Edge {
  std::string id;
  std::string target;
  uint32_t maxPartitions;
  nlohmann::json::object_t applicationConfiguration;
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
  std::vector<Edge> downstream;
  nlohmann::json::object_t applicationConfiguration;
};

/**
 * Class for parsing the application DAG.
 */
class DAGParser {
public:
  DAGParser(const std::string &appName, std::vector<Node> &&nodes);

  DAGParser(const std::string &appName, std::vector<Node> &&nodeList,
            std::optional<nlohmann::json> json);

  static DAGParser parseFromFile(const std::string &filename);

  static DAGParser fromJson(nlohmann::json json);

  const std::vector<Node> &getNodes();

  const std::string &getApplicationName();

  const Node &findNodeByName(const std::string &nodeName);

  const Edge &findEdgeByName(const std::string &edgeId);

  std::vector<std::pair<const Node &, const Edge &>>
  findUpstreamEdges(const std::string &taskId);

  std::vector<std::pair<const Node &, const Edge &>>
  findUpstreamEdges(const Node &node);

  std::optional<nlohmann::json> getRawDag();

private:
  std::string m_applicationName;
  std::vector<Node> m_nodes;

  std::optional<nlohmann::json> m_json;
};

} // namespace iceflow

#endif // DAGPARSER_HPP
