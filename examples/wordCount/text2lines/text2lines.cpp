#include "iceflow/iceflow.hpp"
#include "iceflow/measurements.hpp"
#include "iceflow/producer.hpp"

#include <csignal>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#include <yaml-cpp/yaml.h>

iceflow::Measurement *measurementHandler;

void signalCallbackHandler(int signum) {
  measurementHandler->recordToFile();
  exit(signum);
}

class LineSplitter {
public:
  void text2lines(const std::string &applicationName,
                  const std::string &fileName,
                  std::function<void(std::string)> push) {

    auto application = applicationName;
    std::cout << "Starting " << application << " Application - - - - "
              << std::endl;
    std::ifstream file;
    file.open(fileName);

    if (file.is_open()) {
      int computeCounter = 0;

      for (std::string line; getline(file, line);) {
        measurementHandler->setField(std::to_string(computeCounter),
                                     "CMP_START", 0);
        push(line);
        measurementHandler->setField(std::to_string(computeCounter),
                                     "text->lines1", 0);
        measurementHandler->setField(std::to_string(computeCounter),
                                     "text->lines2", 0);
        measurementHandler->setField(std::to_string(computeCounter),
                                     "CMP_FINISH", 0);
        computeCounter++;
      }
      file.close();
      measurementHandler->setField(std::to_string(computeCounter), "CMP_FINISH",
                                   0);
    } else {
      std::cerr << "Error opening file: " << fileName << std::endl;
    }
  }
};

void run(const std::string &syncPrefix, const std::string &nodePrefix,
         const std::string &pubTopic,
         const std::unordered_set<uint64_t> &topicPartitions,
         const std::string &filename,
         std::chrono::milliseconds publishInterval) {
  std::cout << "Starting IceFlow Stream Processing - - - -" << std::endl;
  LineSplitter lineSplitter;
  ndn::Face face;

  auto iceflow = std::make_shared<iceflow::IceFlow>(syncPrefix, nodePrefix,

                                                    face);
  auto producer = iceflow::IceflowProducer(iceflow, pubTopic, topicPartitions,
                                           publishInterval);

  std::vector<std::thread> threads;
  threads.emplace_back(&iceflow::IceFlow::run, iceflow);
  threads.emplace_back([&lineSplitter, &producer, &nodePrefix, &filename]() {
    lineSplitter.text2lines(
        nodePrefix, filename, [&producer](std::string data) {
          std::vector<uint8_t> encodedString(data.begin(), data.end());
          producer.pushData(encodedString);
        });
  });

  for (auto &thread : threads) {
    thread.join();
  }
}

int main(int argc, const char *argv[]) {
  std::string command = argv[0];

  if (argc != 4) {
    std::cout << "usage: " << command
              << " <config-file> <text-file> <measurement-Name>" << std::endl;
    return 1;
  }

  std::string configFileName = argv[1];
  std::string sourceTextFileName = argv[2];
  std::string measurementFileName = argv[3];

  YAML::Node config = YAML::LoadFile(configFileName);
  YAML::Node producerConfig = config["producer"];
  YAML::Node measurementConfig = config["measurements"];

  auto syncPrefix = config["syncPrefix"].as<std::string>();
  auto nodePrefix = config["nodePrefix"].as<std::string>();
  auto partitions = config["partitions"].as<std::vector<uint64_t>>();
  auto pubTopic = producerConfig["topic"].as<std::string>();
  int publishInterval = producerConfig["publishInterval"].as<int>();
  int saveInterval = measurementConfig["saveInterval"].as<int>();

  ::signal(SIGINT, signalCallbackHandler);
  measurementHandler = new iceflow::Measurement(measurementFileName, nodePrefix,
                                                saveInterval, "A");

  try {
    run(syncPrefix, nodePrefix, pubTopic,
        std::unordered_set(partitions.begin(), partitions.end()),
        sourceTextFileName, std::chrono::milliseconds(publishInterval));
  } catch (const std::exception &e) {
    std::cout << e.what() << std::endl;
  }

  return 0;
}
