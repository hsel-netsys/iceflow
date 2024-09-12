#include "iceflow/consumer.hpp"
#include "iceflow/dag-parser.hpp"
#include "iceflow/iceflow.hpp"
#include "iceflow/measurements.hpp"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <csignal>
#include <iostream>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/logger.hpp>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <yaml-cpp/yaml.h>

NDN_LOG_INIT(iceflow.examples.wordcount);

iceflow::Measurement *measurementHandler;

void signalCallbackHandler(int signum) {
  measurementHandler->recordToFile();
  // Terminate program
  exit(signum);
}

class WordCounter {
public:
  void countWord(std::function<std::string()> receive) {
    int computeCounter = 0;
    while (true) {
      std::string words = receive();
      measurementHandler->setField(std::to_string(computeCounter), "CMP_START",
                                   0);
      measurementHandler->setField(std::to_string(computeCounter),
                                   "text->wordcount", 0);
      std::istringstream stream(words);
      std::string word;
      while (stream >> word) {
        std::cout << "Word occurrences:\n";
        std::transform(word.begin(), word.end(), word.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        // Increment the count for the word in the map
        wordCountMap[word]++;
        measurementHandler->setField(std::to_string(computeCounter),
                                     "lines2->wordcount", 0);
        measurementHandler->setField(std::to_string(computeCounter),
                                     "CMP_FINISH", 0);
        computeCounter++;
        printOccurances();
      }
    }
  }
  void printOccurances() {

    for (const auto &pair : wordCountMap) {
      std::cout << pair.first << ": " << pair.second << " times\n";
    }
  }

private:
  std::unordered_map<std::string, int> wordCountMap;
};

void run(const std::string &syncPrefix, const std::string &nodePrefix,
         const std::string &subTopic,
         std::vector<uint32_t> consumerPartitions) {
  WordCounter compute;
  ndn::Face face;

  auto iceflow =
      std::make_shared<iceflow::IceFlow>(syncPrefix, nodePrefix, face);
  auto consumer =
      iceflow::IceflowConsumer(iceflow, subTopic, consumerPartitions);

  std::vector<std::thread> threads;
  threads.emplace_back(&iceflow::IceFlow::run, iceflow);
  threads.emplace_back([&compute, &consumer]() {
    compute.countWord([&consumer]() -> std::string {
      auto data = consumer.receiveData();
      return std::string(data.begin(), data.end());
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
    std::cout << "usage: " << command << " <<application-dag-file>>"
              << std::endl;
    return 1;
  }

  std::string nodeName = "wordcount";
  std::string dagFileName = argv[1];
  auto dagParser = iceflow::DAGParser::parseFromFile(dagFileName);

  std::string syncPrefix = "/" + dagParser.getApplicationName();
  auto nodePrefix = generateNodePrefix();
  auto node = dagParser.findNodeByName(nodeName);

  auto upstreamEdges = dagParser.findUpstreamEdges(node);
  auto upstreamEdge = upstreamEdges.at(0).second;
  auto upstreamEdgeName = upstreamEdge.id;

  auto applicationConfiguration = node.applicationConfiguration;
  auto saveThreshold =
      applicationConfiguration.at("measurementsSaveThreshold").get<uint64_t>();

  ::signal(SIGINT, signalCallbackHandler);
  measurementHandler =
      new iceflow::Measurement(nodeName, nodePrefix, saveThreshold, "A");

  try {
    std::string subTopic = syncPrefix + "/" + upstreamEdgeName;
    std::vector<uint32_t> consumerPartitions{0};

    run(syncPrefix, nodePrefix, subTopic, consumerPartitions);
  }

  catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}
