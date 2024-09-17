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
                   std::function<void(std::vector<uint8_t>)> push) {
    measurementHandler->setField(std::to_string(m_computeCounter), "CMP_START",
                                 0);
    std::stringstream streamedLines(line);
    std::string word;
    while (streamedLines >> word) {
      std::vector<uint8_t> data(word.begin(), word.end());
      push(data);
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

/**
 * Creates a vector containing every nth partition (denoted by the
 * `numberOfConsumers`) up until the `highestPartitionNumber`, starting with the
 * `consumerIndex`.
 */
std::vector<uint32_t> createConsumerPartitions(uint32_t highestPartitionNumber,
                                               uint32_t consumerIndex,
                                               uint32_t numberOfConsumers) {
  auto consumerPartitions = std::vector<uint32_t>();

  for (auto i = consumerIndex; i <= highestPartitionNumber;
       i += numberOfConsumers) {
    consumerPartitions.push_back(i);
  }

  return consumerPartitions;
}

void run(const std::string &nodeName, const std::string &dagFileName,
         uint32_t consumerIndex, uint32_t numberOfConsumers) {
  WordSplitter wordSplitter;
  ndn::Face face;

  auto dagParser = iceflow::DAGParser::parseFromFile(dagFileName);
  auto node = dagParser.findNodeByName(nodeName);

  auto upstreamEdge = dagParser.findUpstreamEdges(node).at(0).second;
  auto upstreamEdgeName = upstreamEdge.id;
  auto downstreamEdgeName = node.downstream.at(0).id;

  auto consumerPartitions =
      createConsumerPartitions(upstreamEdge.maxPartitions, consumerIndex, 2);

  auto applicationConfiguration = node.applicationConfiguration;
  auto saveThreshold =
      applicationConfiguration.at("measurementsSaveThreshold").get<uint64_t>();

  auto iceflow = std::make_shared<iceflow::IceFlow>(dagParser, nodeName, face);

  ::signal(SIGINT, signalCallbackHandler);
  measurementHandler = new iceflow::Measurement(
      nodeName, iceflow->getNodePrefix(), saveThreshold, "A");

  auto prosumerCallback = [&iceflow, &wordSplitter, &downstreamEdgeName](
                              const std::vector<uint8_t> &data,
                              iceflow::ProducerCallback producerCallback) {
    std::string line(data.begin(), data.end());

    auto pushDataCallback = [downstreamEdgeName,
                             producerCallback](std::vector<uint8_t> data) {
      producerCallback(downstreamEdgeName, data);
    };

    wordSplitter.lines2words(line, pushDataCallback);
  };

  iceflow->registerProsumerCallback(upstreamEdgeName, prosumerCallback);

  iceflow->repartitionConsumer(upstreamEdgeName, consumerPartitions);
  iceflow->run();
}

int main(int argc, const char *argv[]) {

  if (argc != 4) {
    std::string command = argv[0];
    std::cout
        << "usage: " << command
        << " <application-dag-file> <instance-number> <number-of-instances>"
        << std::endl;
    return 1;
  }

  try {
    std::string nodeName = "lines2words";
    std::string dagFileName = argv[1];
    int consumerIndex = std::stoi(argv[2]);
    int numberOfConsumers = std::stoi(argv[3]);

    run(nodeName, dagFileName, consumerIndex, numberOfConsumers);
  }

  catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}
