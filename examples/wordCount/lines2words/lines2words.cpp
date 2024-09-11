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

std::vector<uint32_t> createConsumerPartitions(iceflow::Edge upstreamEdge,
                                               uint32_t consumerIndex) {
  auto maxConsumerPartitions = upstreamEdge.maxPartitions;
  auto consumerPartitions = std::vector<uint32_t>();

  for (auto i = consumerIndex; i < maxConsumerPartitions; i += consumerIndex) {
    consumerPartitions.push_back(i);
  }

  return consumerPartitions;
}

void run(const std::string &nodeName, const std::string &dagFileName,
         uint32_t consumerIndex) {
  WordSplitter wordSplitter;
  ndn::Face face;

  auto dagParser = iceflow::DAGParser::parseFromFile(dagFileName);
  auto node = dagParser.findNodeByName(nodeName);

  auto upstreamEdges = dagParser.findUpstreamEdges(node);
  auto upstreamEdge = upstreamEdges.at(0).second;
  auto upstreamEdgeName = upstreamEdge.id;
  auto consumerPartitions =
      createConsumerPartitions(upstreamEdge, consumerIndex);

  // TODO: Do this dynamically for all edges
  auto downstreamEdge = node.downstream.at(0);
  auto downstreamEdgeName = downstreamEdge.id;
  auto numberOfPartitions = downstreamEdge.maxPartitions;

  auto iceflow = std::make_shared<iceflow::IceFlow>(dagParser, nodeName, face);

  std::string subTopic = iceflow->getSyncPrefix() + "/" + upstreamEdgeName;
  std::string pubTopic = iceflow->getSyncPrefix() + "/" + downstreamEdgeName;

  // TODO: Get rid of this variable eventually
  auto publishInterval = std::chrono::milliseconds(500);

  auto producer = iceflow::IceflowProducer(iceflow, pubTopic,
                                           numberOfPartitions, publishInterval);
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

  try {
    run(nodeName, dagFileName, consumerIndex);
  }

  catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}
