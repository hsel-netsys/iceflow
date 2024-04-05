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
      std::stringstream streamedlines(line);
      std::string word;
      while (streamedlines >> word) {
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
         const std::vector<int> &topicPartitions, int publishInterval) {
  WordSplitter wordSplitter;
  ndn::Face face;
  iceflow::IceFlow iceflow(syncPrefix, nodePrefix, std::optional(subTopic),
                           std::optional(pubTopic), topicPartitions, face,
                           publishInterval);

  std::vector<std::thread> threads;
  threads.emplace_back(&iceflow::IceFlow::run, &iceflow);
  threads.emplace_back([&wordSplitter, &iceflow]() {
    wordSplitter.lines2words(
        [&iceflow]() -> std::string {
          auto data = iceflow.receiveData();
          return std::string(data.begin(), data.end());
        },
        [&iceflow](std::string data) {
          std::vector<uint8_t> encodedString(data.begin(), data.end());
          iceflow.pushData(encodedString);
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
  YAML::Node producerConfig = config["producer"];
  YAML::Node measurementConfig = config["measurements"];

  std::string syncPrefix = config["syncPrefix"].as<std::string>();
  std::string nodePrefix = config["nodePrefix"].as<std::string>();
  std::vector<int> partitions = config["partitions"].as<std::vector<int>>();
  std::string pubTopic = producerConfig["topic"].as<std::string>();
  std::string subTopic = consumerConfig["topic"].as<std::string>();
  int publishInterval = producerConfig["publishInterval"].as<int>();
  int saveInterval = measurementConfig["saveInterval"].as<int>();

  ::signal(SIGINT, signalCallbackHandler);
  measurementHandler = new iceflow::Measurement(measurementFileName, nodePrefix,
                                                saveInterval, "A");

  try {
    run(syncPrefix, nodePrefix, subTopic, pubTopic, partitions,
        publishInterval);
  }

  catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}