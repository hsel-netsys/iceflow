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

#include "dag-Parser.hpp"

namespace iceflow {

NDN_LOG_INIT(iceflow.DAGParser);

DAGParser::DAGParser(const std::string &appName,
                     const std::vector<Node> &nodeList)
    : applicationName(appName), nodes(nodeList) {}

DAGParser DAGParser::parseFromFile(const std::string &filename) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    throw std::runtime_error("File can not be opened " + filename);
  }

  json dagParam;
  file >> dagParam;


  std::string appName = dagParam.at("applicationName").get<std::string>();


  std::vector<Node> nodeList;
  for (const auto &nodeJson : dagParam.at("nodes")) {
    Node nodeInstance;

    // Tasks
    nodeInstance.task = nodeJson.at("task").get<std::string>();
    nodeInstance.name = nodeJson.at("name").get<std::string>();
    nodeInstance.description = nodeJson.at("description").get<std::string>();
    nodeInstance.executor = nodeJson.at("executor").get<std::string>();

    // Container
    nodeInstance.container.image =
        nodeJson.at("container").at("image").get<std::string>();
    nodeInstance.container.resources.cpu =
        nodeJson.at("container").at("resources").at("cpu").get<uint32_t>();
    nodeInstance.container.resources.memory =
        nodeJson.at("container").at("resources").at("memory").get<uint32_t>();

    // Scaling parameters
    nodeInstance.scalingParameters.taskComplexity =
        nodeJson.at("scalingParameters").at("taskComplexity").get<uint32_t>();

    // Communication task with optional payload size
    if (nodeJson.contains("communicationTask") &&
        nodeJson["communicationTask"].contains("payloadSize")) {
      nodeInstance.communicationTask.payloadSize =
          nodeJson["communicationTask"]["payloadSize"].get<uint32_t>();
    }

    // downstream edges
    if (nodeJson.contains("downstream")) {
      std::vector<Edge> edges;
      for (const auto &edgeJson : nodeJson.at("downstream")) {
        Edge edgeInstance;
        edgeInstance.id = edgeJson.at("id").get<std::string>();
        edgeInstance.target = edgeJson.at("target").get<std::string>();
        edgeInstance.maxPartitions =
            edgeJson.at("maxPartitions").get<uint32_t>();
        edges.push_back(edgeInstance);
      }
      nodeInstance.downstream = edges;
    }

    nodeList.push_back(nodeInstance);
  }

  return DAGParser(appName, nodeList);
}

const std::vector<Node> &DAGParser::getNodes() { return nodes; }

const Node &DAGParser::findNodeByName(const std::string &nodeName) {
  auto it =
      std::find_if(nodes.begin(), nodes.end(), [&nodeName](const Node &node) {
        return node.name == nodeName;
      });

  if (it != nodes.end()) {
    return *it;
  } else {
    throw std::runtime_error("Node with name '" + nodeName + "' not found");
  }
}

const Edge &DAGParser::findEdgeByName(const std::string &edgeId) {
  for (const auto &node : nodes) {
    if (node.downstream.has_value()) {
      auto it = std::find_if(
          node.downstream->begin(), node.downstream->end(),
          [&edgeId](const Edge &edge) { return edge.id == edgeId; });

      if (it != node.downstream->end()) {
        return *it;
      }
    }
  }
  throw std::runtime_error("Edge with ID '" + edgeId + "' not found");
}

std::vector<std::pair<const Node &, const Edge &>>
DAGParser::findUpstreamEdges(const std::string &nodeName) {
  std::vector<std::pair<const Node &, const Edge &>> upstreamEdges;

  for (const auto &node : nodes) {
    if (node.downstream.has_value()) {
      auto it = std::find_if(
          node.downstream->begin(), node.downstream->end(),
          [&nodeName](const Edge &edge) { return edge.target == nodeName; });

      if (it != node.downstream->end()) {
        upstreamEdges.emplace_back(node, *it);
      }
    }
  }

  return upstreamEdges;
}

} // namespace iceflow