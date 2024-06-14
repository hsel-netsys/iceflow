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
         const std::string &pubTopic, uint32_t numberOfPartitions,
         const std::string &filename,
         std::chrono::milliseconds publishInterval) {
  std::cout << "Starting IceFlow Stream Processing - - - -" << std::endl;
  LineSplitter lineSplitter;
  ndn::Face face;

  auto iceflow = std::make_shared<iceflow::IceFlow>(syncPrefix, nodePrefix,

                                                    face);
  auto producer = iceflow::IceflowProducer(iceflow, pubTopic,
                                           numberOfPartitions, publishInterval);

  std::vector<std::thread> threads;
  threads.emplace_back(&iceflow::IceFlow::run, iceflow);
  threads.emplace_back([&lineSplitter, &producer, &nodePrefix, &filename]() {
    lineSplitter.text2lines(
        nodePrefix, filename, [&producer](const std::string &data) {
          std::vector<uint8_t> encodedString(data.begin(), data.end());
          producer.pushData(encodedString);
        });
  });

  for (auto &thread : threads) {
    thread.join();
  }
}

int main(int argc, const char *argv[]) {

  if (argc != 4) {
    std::string command = argv[0];
    std::cout << "usage: " << command
              << " <config-file> <text-file> <measurement-Name>" << std::endl;
    return 1;
  }

  std::string configFileName = argv[1];
  std::string measurementFileName = argv[3];

  YAML::Node config = YAML::LoadFile(configFileName);
  YAML::Node producerConfig = config["producer"];
  YAML::Node measurementConfig = config["measurements"];

  auto syncPrefix = config["syncPrefix"].as<std::string>();
  auto nodePrefix = config["nodePrefix"].as<std::string>();
  auto pubTopic = producerConfig["topic"].as<std::string>();
  uint64_t publishInterval = producerConfig["publishInterval"].as<uint64_t>();
  auto numberOfPartitions = producerConfig["numberOfPartitions"].as<uint32_t>();
  uint64_t saveThreshold = measurementConfig["saveThreshold"].as<uint64_t>();

  ::signal(SIGINT, signalCallbackHandler);
  measurementHandler = new iceflow::Measurement(measurementFileName, nodePrefix,
                                                saveThreshold, "A");

  try {
    std::string sourceTextFileName = argv[2];
    run(syncPrefix, nodePrefix, pubTopic, numberOfPartitions,
        sourceTextFileName, std::chrono::milliseconds(publishInterval));
  } catch (const std::exception &e) {
    std::cout << e.what() << std::endl;
  }

  return 0;
}
