#include "ndn-cxx/util/logger.hpp"

#include "dagParser.hpp"

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

  // Parsing the application name
  std::string appName = dagParam.at("applicationName").get<std::string>();

  // Parsing the node parameters
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

    // Optional downstream nodes
    if (nodeJson.contains("downstream")) {
      std::vector<Edge> edges;
      for (const auto &edgeJson : nodeJson.at("downstream")) {
        Edge edgeInstance;
        edgeInstance.id = edgeJson.at("id").get<std::string>();
        edgeInstance.target = edgeJson.at("target").get<std::string>();
        edgeInstance.max_partitions =
            edgeJson.at("max_partitions").get<uint32_t>();
        edges.push_back(edgeInstance);
      }
      nodeInstance.downstream = edges;
    }

    nodeList.push_back(nodeInstance);
  }

  return DAGParser(appName, nodeList);
}

void DAGParser::printNodeDetails() {
  std::cout << "Application Name: " << applicationName << std::endl;

  for (const auto &node : nodes) {
    std::cout << "Task: " << node.task << ", Name: " << node.name << std::endl;
    std::cout << "  Description: " << node.description << std::endl;
    std::cout << "  Executor: " << node.executor << std::endl;
    std::cout << "  Container Image: " << node.container.image << std::endl;
    std::cout << "  Resources - CPU: " << node.container.resources.cpu
              << ", Memory: " << node.container.resources.memory << std::endl;
    std::cout << "  Task Complexity: " << node.scalingParameters.taskComplexity
              << std::endl;

    if (node.communicationTask.payloadSize.has_value()) {
      std::cout << "  Payload Size: "
                << node.communicationTask.payloadSize.value() << std::endl;
    }

    if (node.downstream.has_value()) {
      std::cout << "  Downstream: " << std::endl;
      for (const auto &edge : node.downstream.value()) {
        std::cout << "    Edge ID: " << edge.id << ", Target: " << edge.target
                  << ", Max Partitions: " << edge.max_partitions << std::endl;
      }
    }
  }
}

const std::vector<Node> &DAGParser::getNodes() { return nodes; }

const Node &DAGParser::findNodeByName(const std::string &nodeName) {
  for (const auto &node : nodes) {
    if (node.name == nodeName) {
      return node;
    }
  }
  throw std::runtime_error("Node with name '" + nodeName + "' not found");
}

const Edge &DAGParser::findEdgeByName(const std::string &edgeId) {
  for (const auto &node : nodes) {
    if (node.downstream.has_value()) {
      for (const auto &edge : node.downstream.value()) {
        if (edge.id == edgeId) {
          return edge;
        }
      }
    }
  }
  throw std::runtime_error("Edge with ID '" + edgeId + "' not found");
}

std::vector<std::pair<const Node &, const Edge &>>
DAGParser::findUpstreamEdges(const std::string &node_name) {
  std::vector<std::pair<const Node &, const Edge &>> upstreamEdges;

  for (const auto &node : nodes) {
    if (node.downstream.has_value()) {
      for (const auto &edge : node.downstream.value()) {
        if (edge.target == node_name) {
          upstreamEdges.emplace_back(node, edge);
        }
      }
    }
  }

  return upstreamEdges;
}

} // namespace iceflow
