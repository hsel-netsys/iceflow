#include "iceflow/consumer.hpp"
#include "iceflow/dag-parser.hpp"
#include "iceflow/iceflow.hpp"
#include "iceflow/measurements.hpp"
#include "iceflow/producer.hpp"

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

NDN_LOG_INIT(iceflow.examples.lines2words);

iceflow::Measurement *measurementHandler;

void signalCallbackHandler(int signum) {
  measurementHandler->recordToFile();
  exit(signum);
}

class WordSplitter {
public:
  void lines2words(std::function<std::string()> receive,
                   std::function<void(std::string)> push) {
    int computeCounter = 0;
    while (true) {
      auto line = receive();
      measurementHandler->setField(std::to_string(computeCounter), "CMP_START",
                                   0);
      std::stringstream streamedLines(line);
      std::string word;
      while (streamedLines >> word) {
        std::cout << word << std::endl;
        push(word);
        measurementHandler->setField(std::to_string(computeCounter),
                                     "lines1->words", 0);
        measurementHandler->setField(std::to_string(computeCounter),
                                     "CMP_FINISH", 0);
      }
    }
  }
};

void run(const std::string &syncPrefix, const std::string &nodePrefix,
         const std::string &subTopic, const std::string &pubTopic,
         uint32_t numberOfProducerPartitions,
         std::vector<uint32_t> consumerPartitions,
         std::chrono::milliseconds publishInterval) {
  WordSplitter wordSplitter;
  ndn::Face face;
  auto iceflow =
      std::make_shared<iceflow::IceFlow>(syncPrefix, nodePrefix, face);
  auto producer = iceflow::IceflowProducer(
      iceflow, pubTopic, numberOfProducerPartitions, publishInterval);
  auto consumer =
      iceflow::IceflowConsumer(iceflow, subTopic, consumerPartitions);

  std::vector<std::thread> threads;
  threads.emplace_back(&iceflow::IceFlow::run, iceflow);
  threads.emplace_back([&wordSplitter, &consumer, &producer]() {
    wordSplitter.lines2words(
        [&consumer]() -> std::string {
          auto data = consumer.receiveData();
          return std::string(data.begin(), data.end());
        },
        [&producer](const std::string &data) {
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

std::vector<uint32_t> createConsumerPartitions(iceflow::Edge upstreamEdge,
                                               uint32_t consumerIndex) {
  auto maxConsumerPartitions = upstreamEdge.maxPartitions;
  auto consumerPartitions = std::vector<uint32_t>();

  for (auto i = consumerIndex; i < maxConsumerPartitions; i += consumerIndex) {
    consumerPartitions.push_back(i);
  }

  return consumerPartitions;
}

int main(int argc, const char *argv[]) {

  if (argc != 3) {
    std::string command = argv[0];
    std::cout << "usage: " << command
              << " <application-dag-file> <instance-number>" << std::endl;
    return 1;
  }

  std::string nodeName = "lines2words";
  std::string dagFileName = argv[1];
  int consumerIndex = std::stoi(argv[2]);
  auto dagParser = iceflow::DAGParser::parseFromFile(dagFileName);

  std::string syncPrefix = "/" + dagParser.getApplicationName();
  auto nodePrefix = generateNodePrefix();
  auto node = dagParser.findNodeByName(nodeName);
  // TODO: Should this method find edges by node name or task ID?
  auto upstreamEdges = dagParser.findUpstreamEdges(node.task);

  // TODO: Do this dynamically for all edges
  auto downstreamEdge = node.downstream.at(0);
  auto downstreamEdgeName = downstreamEdge.id;
  auto numberOfProducerPartitions = downstreamEdge.maxPartitions;

  auto applicationConfiguration = node.applicationConfiguration;
  auto saveThreshold =
      applicationConfiguration.at("measurementsSaveThreshold").get<uint64_t>();

  auto upstreamEdge = upstreamEdges.at(0).second;
  auto upstreamEdgeName = upstreamEdge.id;
  auto consumerPartitions =
      createConsumerPartitions(upstreamEdge, consumerIndex);

  ::signal(SIGINT, signalCallbackHandler);
  measurementHandler =
      new iceflow::Measurement(nodeName, nodePrefix, saveThreshold, "A");

  try {
    // TODO: Get rid of this variable eventually
    auto publishInterval = 500;

    std::string subTopic = syncPrefix + "/" + upstreamEdgeName;
    std::string pubTopic = syncPrefix + "/" + downstreamEdgeName;

    run(syncPrefix, nodePrefix, subTopic, pubTopic, numberOfProducerPartitions,
        consumerPartitions, std::chrono::milliseconds(publishInterval));
  }

  catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}
