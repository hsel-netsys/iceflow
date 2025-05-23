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

#include "dag-parser.hpp"

namespace iceflow {

NDN_LOG_INIT(iceflow.DAGParser);

DAGParser::DAGParser(const std::string &appName, std::vector<Node> &&nodeList,
                     std::optional<nlohmann::json> json)
    : m_applicationName(appName), m_nodes(std::move(nodeList)), m_json(json) {}

DAGParser::DAGParser(const std::string &appName, std::vector<Node> &&nodeList)
    : DAGParser(appName, std::move(nodeList), std::nullopt) {}

DAGParser DAGParser::fromJson(nlohmann::json json) {
  std::string appName = json.at("applicationName").get<std::string>();

  std::vector<Node> nodeList;
  for (const auto &nodeJson : json.at("nodes")) {
    Node nodeInstance;

    // Tasks
    nodeInstance.task = nodeJson.at("task").get<std::string>();
    nodeInstance.name = nodeJson.at("name").get<std::string>();
    nodeInstance.description = nodeJson.at("description").get<std::string>();
    nodeInstance.executor = nodeJson.at("executor").get<std::string>();

    // Container
    auto containerJson = nodeJson.at("container");

    nodeInstance.container.image = containerJson.at("image").get<std::string>();
    nodeInstance.container.tag = containerJson.value("tag", "latest");
    nodeInstance.container.envs = std::map<std::string, std::string>();
    auto envs = containerJson.value("envs", json::object());
    for (const auto &[key, value] : envs.items()) {
      nodeInstance.container.envs.emplace(key, value.get<std::string>());
    }
    auto mounts = containerJson.value("mounts", json::object());
    for (const auto &[key, value] : mounts.items()) {
      nodeInstance.container.mounts.emplace(key, value.get<std::string>());
    }
    nodeInstance.container.resources.cpu =
        containerJson.at("resources").at("cpu").get<uint32_t>();
    nodeInstance.container.resources.memory =
        containerJson.at("resources").at("memory").get<uint32_t>();

    // Scaling parameters
    nodeInstance.scalingParameters.taskComplexity =
        nodeJson.at("scalingParameters").at("taskComplexity").get<uint32_t>();

    // downstream edges
    if (nodeJson.contains("downstream")) {
      std::vector<Edge> edges;
      for (const auto &edgeJson : nodeJson.at("downstream")) {
        Edge edgeInstance;
        edgeInstance.id = edgeJson.at("id").get<std::string>();
        edgeInstance.target = edgeJson.at("target").get<std::string>();
        edgeInstance.maxPartitions =
            edgeJson.at("maxPartitions").get<uint32_t>();
        edgeInstance.applicationConfiguration = edgeJson.value(
            "applicationConfiguration", nlohmann::json::object());
        edges.push_back(edgeInstance);
      }
      nodeInstance.downstream = edges;
    }

    nodeInstance.applicationConfiguration =
        nodeJson.value("applicationConfiguration", nlohmann::json::object());

    nodeList.push_back(nodeInstance);
  }

  return DAGParser(appName, std::move(nodeList),
                   std::optional<nlohmann::json>(json));
}

DAGParser DAGParser::parseFromFile(const std::string &filename) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    throw std::runtime_error("File can not be opened " + filename);
  }

  json dagParam;
  file >> dagParam;

  return DAGParser::fromJson(dagParam);
}

const std::vector<Node> &DAGParser::getNodes() { return m_nodes; }

const std::string &DAGParser::getApplicationName() { return m_applicationName; }

const Node &DAGParser::findNodeByName(const std::string &nodeName) {
  auto it = std::find_if(
      m_nodes.begin(), m_nodes.end(),
      [&nodeName](const Node &node) { return node.name == nodeName; });

  if (it != m_nodes.end()) {
    return *it;
  }

  throw std::runtime_error("Node with name '" + nodeName + "' not found");
}

const Edge &DAGParser::findEdgeByName(const std::string &edgeId) {
  for (const auto &node : m_nodes) {
    auto downstream = node.downstream;

    auto it =
        std::find_if(downstream.begin(), downstream.end(),
                     [&edgeId](const Edge &edge) { return edge.id == edgeId; });

    if (it != downstream.end()) {
      return *it;
    }
  }

  throw std::runtime_error("Edge with ID '" + edgeId + "' not found");
}

std::vector<std::pair<const Node &, const Edge &>>
DAGParser::findUpstreamEdges(const Node &node) {
  return findUpstreamEdges(node.task);
}

std::vector<std::pair<const Node &, const Edge &>>
DAGParser::findUpstreamEdges(const std::string &taskId) {
  std::vector<std::pair<const Node &, const Edge &>> upstreamEdges;

  for (const auto &node : m_nodes) {

    auto it = std::find_if(
        node.downstream.begin(), node.downstream.end(),
        [&taskId](const Edge &edge) { return edge.target == taskId; });

    if (it != node.downstream.end()) {
      upstreamEdges.emplace_back(node, *it);
    }
  }

  return upstreamEdges;
}

std::optional<nlohmann::json> DAGParser::getRawDag() { return m_json; }

} // namespace iceflow
