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

// Paramters of a node in the DAG
struct Node {
  std::string task;
  std::string name;
  std::string description;
  std::string executor;
  Container container;
  ScalingParameters scalingParameters;
  CommunicationTask communicationTask;
  std::optional<std::vector<std::string>> downstream;
};

// Class  for parsing the DAG application
class DAGParser {
public:
  std::string applicationName;
  std::vector<Node> nodes;

  static DAGParser parseFromFile(const std::string &filename);
  void printNodeDetails();
};

} // namespace iceflow

#endif // DAGPARSER_HPP
