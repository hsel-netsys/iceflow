#include "iceflow/dag-parser.hpp"
#include "iceflow/iceflow.hpp"
#include "iceflow/measurements.hpp"
#include "iceflow/producer.hpp"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <csignal>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#include <yaml-cpp/yaml.h>

iceflow::Measurement *measurementHandler;

void signalCallbackHandler(int signum) {
  measurementHandler->recordToFile();
  exit(signum);
}

class LineSplitter {
public:
  void text2lines(const std::string &applicationName,
                  const std::string &fileName,
                  std::function<void(std::string)> push) {

    auto application = applicationName;
    std::cout << "Starting " << application << " Application - - - - "
              << std::endl;
    std::ifstream file;
    file.open(fileName);

    if (file.is_open()) {
      int computeCounter = 0;

      for (std::string line; getline(file, line);) {
        measurementHandler->setField(std::to_string(computeCounter),
                                     "CMP_START", 0);
        push(line);
        measurementHandler->setField(std::to_string(computeCounter),
                                     "text->lines1", 0);
        measurementHandler->setField(std::to_string(computeCounter),
                                     "text->lines2", 0);
        measurementHandler->setField(std::to_string(computeCounter),
                                     "CMP_FINISH", 0);
        computeCounter++;
      }
      file.close();
      measurementHandler->setField(std::to_string(computeCounter), "CMP_FINISH",
                                   0);
    } else {
      std::cerr << "Error opening file: " << fileName << std::endl;
    }
  }
};

void run(const std::string &nodeName, const std::string &dagFileName) {
  std::cout << "Starting IceFlow Stream Processing - - - -" << std::endl;
  LineSplitter lineSplitter;
  ndn::Face face;

  auto dagParser = iceflow::DAGParser::parseFromFile(dagFileName);

  auto iceflow = std::make_shared<iceflow::IceFlow>(dagParser, nodeName, face);

  std::string syncPrefix = "/" + dagParser.getApplicationName();
  auto nodePrefix = generateNodePrefix();
  auto node = dagParser.findNodeByName(nodeName);

  // TODO: Do this dynamically for all edges
  auto downstreamEdge = node.downstream.at(0);
  auto downstreamEdgeName = downstreamEdge.id;
  auto numberOfPartitions = downstreamEdge.maxPartitions;

  auto applicationConfiguration = node.applicationConfiguration;

  auto saveThreshold =
      applicationConfiguration.at("measurementsSaveThreshold").get<uint64_t>();

  ::signal(SIGINT, signalCallbackHandler);
  measurementHandler =
      new iceflow::Measurement(nodeName, nodePrefix, saveThreshold, "A");
  // TODO: Get rid of this variable eventually
  auto publishInterval = 500;

  std::string pubTopic = syncPrefix + "/" + downstreamEdgeName;

  auto sourceTextFileName =
      applicationConfiguration.at("sourceTextFileName").get<std::string>();

  auto producer = iceflow::IceflowProducer(iceflow, pubTopic,
                                           numberOfPartitions, publishInterval);

  std::vector<std::thread> threads;
  threads.emplace_back(&iceflow::IceFlow::run, iceflow);
  threads.emplace_back([&lineSplitter, &producer, &nodePrefix, &filename]() {
    lineSplitter.text2lines(
        nodePrefix, filename, [&producer](const std::string &data) {
          std::vector<uint8_t> encodedString(data.begin(), data.end());
          producer.pushData(encodedString);
        });
  });

  for (auto &thread : threads) {
    thread.join();
  }
}

std::string generateNodePrefix() {
  boost::uuids::basic_random_generator<boost::mt19937> gen;
  boost::uuids::uuid uuid = gen();
  return to_string(uuid);
}

int main(int argc, const char *argv[]) {

  if (argc != 2) {
    std::string command = argv[0];
    std::cout << "usage: " << command << " <application-dag-file>" << std::endl;
    return 1;
  }

  std::string nodeName = "text2lines";
  std::string dagFileName = argv[1];
  auto dagParser = iceflow::DAGParser::parseFromFile(dagFileName);

  try {
    run(syncPrefix, nodePrefix, pubTopic, numberOfPartitions,
        sourceTextFileName, std::chrono::milliseconds(publishInterval));
  } catch (const std::exception &e) {
    std::cout << e.what() << std::endl;
  }

  return 0;
}
