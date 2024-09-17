#include "iceflow/consumer.hpp"
#include "iceflow/iceflow.hpp"
#include "iceflow/measurements.hpp"
#include "iceflow/serde.hpp"

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/logger.hpp>

#include <csignal>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <yaml-cpp/yaml.h>

NDN_LOG_INIT(iceflow.examples.visionprocessing.aggregate);
iceflow::Measurement *measurementHandler;

void signalCallbackHandler(int signum) {
  measurementHandler->recordToFile();
  std::exit(signum);
}

class Aggregate {
public:
  void aggregate(std::function<std::vector<uint8_t>()> receive1,
                 std::function<std::vector<uint8_t>()> receive2,
                 std::function<std::vector<uint8_t>()> receive3) {
    int computeCounter = 0;

    while (true) {

      analysisStorage.push_back(receive1());
      analysisStorage.push_back(receive2());
      analysisStorage.push_back(receive3());

      std::string computeCounterStr = std::to_string(computeCounter);
      measurementHandler->setField(computeCounterStr, "CMP_START", 0);
      measurementHandler->setField(computeCounterStr, "PC->AGG", 0);

      std::map<int, std::tuple<std::string, std::string, std::string>>
          frameData;

      for (auto &analysis : analysisStorage) {
        nlohmann::json deserializedData = Serde::deserialize(analysis);

        if (deserializedData.contains("frameID")) {
          int frameID = deserializedData["frameID"];

          auto &dataTuple = frameData[frameID];

          if (deserializedData.contains("Age")) {
            std::get<0>(dataTuple) = deserializedData["Age"].get<std::string>();
          }
          if (deserializedData.contains("Gender")) {
            std::get<1>(dataTuple) =
                deserializedData["Gender"].get<std::string>();
          }
          if (deserializedData.contains("PeopleCount")) {
            std::get<2>(dataTuple) =
                deserializedData["PeopleCount"].get<std::string>();
          }
        }
      }

      for (const auto &[frameID, data] : frameData) {
        std::string age = std::get<0>(data).empty() ? "N/A" : std::get<0>(data);
        std::string gender =
            std::get<1>(data).empty() ? "N/A" : std::get<1>(data);
        std::string peopleCount =
            std::get<2>(data).empty() ? "N/A" : std::get<2>(data);

        std::cout << "Frame ID: " << frameID << "\n"
                  << "  Age: " << age << "\n"
                  << "  Gender: " << gender << "\n"
                  << "  PeopleCount: " << peopleCount << "\n"
                  << std::endl;
      }

      measurementHandler->setField(computeCounterStr, "CMP_FINISH", 0);
      computeCounter++;

      analysisStorage.clear();
    }
  }

private:
  std::vector<std::vector<uint8_t>> analysisStorage;
};

void run(const std::string &syncPrefix, const std::string &nodePrefix,
         const std::string &subTopic1,
         std::vector<uint32_t> consumerPartitions1,
         const std::string &subTopic2,
         std::vector<uint32_t> consumerPartitions2,
         const std::string &subTopic3,
         std::vector<uint32_t> consumerPartitions3) {

  Aggregate analyzer;
  ndn::Face face;
  auto iceflow =
      std::make_shared<iceflow::IceFlow>(syncPrefix, nodePrefix, face);

  auto consumer1 =
      iceflow::IceflowConsumer(iceflow, subTopic1, consumerPartitions1);
  auto consumer2 =
      iceflow::IceflowConsumer(iceflow, subTopic2, consumerPartitions2);
  auto consumer3 =
      iceflow::IceflowConsumer(iceflow, subTopic3, consumerPartitions3);

  std::vector<std::thread> threads;

  threads.emplace_back(&iceflow::IceFlow::run, iceflow);
  threads.emplace_back([&analyzer, &consumer1, &consumer2, &consumer3]() {
    analyzer.aggregate([&consumer1]() { return consumer1.receiveData(); },
                       [&consumer2]() { return consumer2.receiveData(); },
                       [&consumer3]() { return consumer3.receiveData(); });
  });

  for (auto &thread : threads) {
    thread.join();
  }
}

int main(int argc, const char *argv[]) {

  if (argc != 3) {
    std::cout << "usage: " << argv[0] << " <config-file> <measurement-file>"
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
  std::string subTopic1 = consumerConfig["topic1"].as<std::string>();
  std::string subTopic2 = consumerConfig["topic2"].as<std::string>();
  std::string subTopic3 = consumerConfig["topic3"].as<std::string>();
  auto consumerPartitions1 =
      consumerConfig["partitions1"].as<std::vector<uint32_t>>();
  auto consumerPartitions2 =
      consumerConfig["partitions2"].as<std::vector<uint32_t>>();
  auto consumerPartitions3 =
      consumerConfig["partitions3"].as<std::vector<uint32_t>>();
  uint64_t saveThreshold = measurementConfig["saveThreshold"].as<uint64_t>();

  ::signal(SIGINT, signalCallbackHandler);
  measurementHandler = new iceflow::Measurement(measurementFileName, nodePrefix,
                                                saveThreshold, "A");

  try {
    run(syncPrefix, nodePrefix, subTopic1, consumerPartitions1, subTopic2,
        consumerPartitions2, subTopic3, consumerPartitions3);
  } catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}
