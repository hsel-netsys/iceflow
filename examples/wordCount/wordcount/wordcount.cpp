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

void run(const std::string &nodeName, const std::string &dagFileName) {
  WordCounter compute;
  ndn::Face face;

  auto dagParser = iceflow::DAGParser::parseFromFile(dagFileName);

  std::unordered_map<std::string, std::function<void(std::vector<uint8_t>)>>
      consumerCallbacks = {{"l2w", [&compute](std::vector<uint8_t> data) {
                              compute.countWord([&data]() -> std::string {
                                return std::string(data.begin(), data.end());
                              });
                            }}};

  auto iceflow = std::make_shared<iceflow::IceFlow>(dagParser, nodeName, face,
                                                    consumerCallbacks

  );

  std::vector<std::thread> threads;
  threads.emplace_back(&iceflow::IceFlow::run, iceflow);

  for (auto &thread : threads) {
    thread.join();
  }
}

int main(int argc, const char *argv[]) {

  if (argc != 2) {
    std::string command = argv[0];
    std::cout << "usage: " << command << " <<application-dag-file>>"
              << std::endl;
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
