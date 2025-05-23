#include "iceflow/consumer.hpp"
#include "iceflow/dag-parser.hpp"
#include "iceflow/iceflow.hpp"
#include "iceflow/measurements.hpp"

#ifdef USE_GRPC
#include "iceflow/scaler.hpp"
#endif

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
    std::string words = receive();
    measurementHandler->setField(std::to_string(m_computeCounter), "CMP_START",
                                 0);
    measurementHandler->setField(std::to_string(m_computeCounter),
                                 "text->wordcount", 0);
    std::istringstream stream(words);
    std::string word;
    while (stream >> word) {
      std::cout << "Word occurrences:" << std::endl;
      std::transform(word.begin(), word.end(), word.begin(),
                     [](unsigned char c) { return std::tolower(c); });

      wordCountMap[word]++;
      measurementHandler->setField(std::to_string(m_computeCounter),
                                   "lines2->wordcount", 0);
      measurementHandler->setField(std::to_string(m_computeCounter),
                                   "CMP_FINISH", 0);
      m_computeCounter++;
      printOccurances();
    }
  }
  void printOccurances() {

    for (const auto &pair : wordCountMap) {
      std::cout << pair.first << ": " << pair.second << " times" << std::endl;
    }
  }

private:
  std::unordered_map<std::string, int> wordCountMap;
  int m_computeCounter = 0;
};

void run(const std::string &nodeName, const std::string &dagFileName) {
  WordCounter compute;
  ndn::Face face;

  auto iceflow =
      std::make_shared<iceflow::IceFlow>(dagFileName, nodeName, face);
  auto upstreamEdgeName = iceflow->getUpstreamEdge(0).value().id;

  auto saveThreshold =
      iceflow->getApplicationParameter<uint64_t>("measurementsSaveThreshold")
          .value();

  ::signal(SIGINT, signalCallbackHandler);
  measurementHandler = new iceflow::Measurement(
      nodeName, iceflow->getNodePrefix(), saveThreshold, "A");

  iceflow->registerConsumerCallback(
      upstreamEdgeName, [&compute](std::vector<uint8_t> data) {
        compute.countWord([&data]() -> std::string {
          return std::string(data.begin(), data.end());
        });
      });

  iceflow->repartitionConsumer(upstreamEdgeName, {0});

#ifdef USE_GRPC
  auto grpcPath =
      iceflow->getApplicationParameter<std::string>("grpcPath").value();
  auto scaler =
      iceflow::IceflowScaler(grpcPath + "/node-instance.sock",
                             grpcPath + "/node-executor.sock", iceflow);
#endif

  iceflow->run();
}

int main(int argc, const char *argv[]) {

  if (argc != 2) {
    std::string command = argv[0];
    std::cout << "usage: " << command << " <application-dag-file>" << std::endl;
    return 1;
  }

  try {
    std::string nodeName = "wordcount";
    std::string dagFileName = argv[1];

    run(nodeName, dagFileName);
  }

  catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}
