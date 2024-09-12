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
  void lines2words(const std::string &line,
                   std::function<void(std::string)> push) {
    measurementHandler->setField(std::to_string(m_computeCounter), "CMP_START",
                                 0);
    std::stringstream streamedLines(line);
    std::string word;
    while (streamedLines >> word) {
      push(word);
      measurementHandler->setField(std::to_string(m_computeCounter),
                                   "lines1->words", 0);
      measurementHandler->setField(std::to_string(m_computeCounter),
                                   "CMP_FINISH", 0);
      m_computeCounter++;
    }
  }

private:
  int m_computeCounter = 0;
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

  auto downstreamEdge = node.downstream.at(0);
  auto downstreamEdgeName = downstreamEdge.id;

  auto applicationConfiguration = node.applicationConfiguration;

  auto saveThreshold =
      applicationConfiguration.at("measurementsSaveThreshold").get<uint64_t>();

  auto iceflow = std::make_shared<iceflow::IceFlow>(dagParser, nodeName, face);
  auto nodePrefix = iceflow->getNodePrefix();

  ::signal(SIGINT, signalCallbackHandler);
  measurementHandler =
      new iceflow::Measurement(nodeName, nodePrefix, saveThreshold, "A");

  auto producerCallback = [&iceflow,
                           &downstreamEdgeName](const std::string &word) {
    std::vector<uint8_t> data(word.begin(), word.end());

    iceflow->pushData(downstreamEdgeName, data);
  };

  auto consumerCallback = [&iceflow, &wordSplitter,
                           &producerCallback](std::vector<uint8_t> data) {
    std::string line(data.begin(), data.end());
    wordSplitter.lines2words(line, producerCallback);
  };

  iceflow->registerConsumerCallback(upstreamEdgeName, consumerCallback);

  iceflow->repartitionConsumer(upstreamEdgeName, consumerPartitions);
  iceflow->run();
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
