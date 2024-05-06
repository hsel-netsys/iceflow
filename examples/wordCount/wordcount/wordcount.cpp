#include "iceflow/consumer.hpp"
#include "iceflow/iceflow.hpp"
#include "iceflow/measurements.hpp"

#include <csignal>
#include <iostream>
#include <ndn-cxx/face.hpp>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <yaml-cpp/yaml.h>

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
        // Convert the word to lowercase to make the counting case-insensitive
        for (char &c : word) {
          c = std::tolower(c);
        }

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
         const std::string &subTopic, const std::vector<int> &topicPartitions) {
  WordCounter compute;
  ndn::Face face;

  auto iceflow =
      std::make_shared<iceflow::IceFlow>(syncPrefix, nodePrefix, face);
  auto consumer = iceflow::IceflowConsumer(iceflow, subTopic, topicPartitions);

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

int main(int argc, char *argv[]) {
  std::string command = argv[0];

  if (argc != 3) {
    std::cout << "usage: " << command << " <config-file> <measurement-Name>"
              << std::endl;
    return 1;
  }

  std::string configFileName = argv[1];
  std::string measurementFileName = argv[2];

  YAML::Node config = YAML::LoadFile(configFileName);
  YAML::Node consumerConfig = config["consumer"];
  YAML::Node measurementConfig = config["measurements"];

  std::string syncPrefix = config["syncPrefix"].as<std::string>();
  std::string nodePrefix = config["nodePrefix"].as<std::string>();
  std::vector<int> partitions = config["partitions"].as<std::vector<int>>();
  std::string subTopic = consumerConfig["topic"].as<std::string>();

  int saveInterval = measurementConfig["saveInterval"].as<int>();

  ::signal(SIGINT, signalCallbackHandler);
  measurementHandler = new iceflow::Measurement(measurementFileName, nodePrefix,
                                                saveInterval, "A");

  try {
    run(syncPrefix, nodePrefix, subTopic, partitions);
  }

  catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}
