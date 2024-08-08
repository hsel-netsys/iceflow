#include "iceflow/consumer.hpp"
#include "iceflow/iceflow.hpp"
#include "iceflow/measurements.hpp"
#include "iceflow/producer.hpp"

#include <csignal>
#include <iostream>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/logger.hpp>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <yaml-cpp/yaml.h>

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

int main(int argc, const char *argv[]) {

  if (argc != 3) {
    std::string command = argv[0];
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
  std::string pubTopic = producerConfig["topic"].as<std::string>();
  std::string subTopic = consumerConfig["topic"].as<std::string>();
  auto consumerPartitions =
      consumerConfig["partitions"].as<std::vector<uint32_t>>();
  auto numberOfProducerPartitions =
      producerConfig["numberOfPartitions"].as<uint32_t>();
  uint64_t publishInterval = producerConfig["publishInterval"].as<uint64_t>();
  uint64_t saveThreshold = measurementConfig["saveThreshold"].as<uint64_t>();

  ::signal(SIGINT, signalCallbackHandler);
  measurementHandler = new iceflow::Measurement(measurementFileName, nodePrefix,
                                                saveThreshold, "A");

  try {
    run(syncPrefix, nodePrefix, subTopic, pubTopic, numberOfProducerPartitions,
        consumerPartitions, std::chrono::milliseconds(publishInterval));
  }

  catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}
