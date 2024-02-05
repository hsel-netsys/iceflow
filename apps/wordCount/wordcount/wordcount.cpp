#include "iceflow/Consumer.hpp"
#include "iceflow/measurements.hpp"

#include <iostream>
#include <ndn-cxx/face.hpp>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <yaml-cpp/yaml.h>

// ###### MEASUREMENT ######
iceflow::Measurement *msCmp;

void signalCallbackHandler(int signum) {
  msCmp->recordToFile();
  // Terminate program
  exit(signum);
}

class Compute {
public:
  void countWord(std::function<std::string()> receive) {
    int computeCounter = 0;
    while (true) {
      std::string words = receive();
      msCmp->setField(std::to_string(computeCounter), "CMP_START", 0);
      msCmp->setField(std::to_string(computeCounter), "text->wordcount", 0);
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
        msCmp->setField(std::to_string(computeCounter), "lines2->wordcount", 0);
        msCmp->setField(std::to_string(computeCounter), "CMP_FINISH", 0);
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

void DataFlow(const std::string &sub_syncPrefix,
              const std::string &sub_Prefix_data_main,
              const std::vector<int> &nDataStreams) {
  Compute compute;
  ndn::Face consumerInterFace;

  iceflow::Consumer consumer(sub_syncPrefix, sub_Prefix_data_main, nDataStreams,
                             consumerInterFace);

  std::vector<std::thread> ConThreads;
  ConThreads.emplace_back(&iceflow::Consumer::run, &consumer);
  ConThreads.emplace_back([&compute, &consumer]() {
    compute.countWord(
        [&consumer]() -> std::string { return consumer.receive(); });
  });

  for (auto &t : ConThreads) {
    t.join();
  }
}

int main(int argc, char *argv[]) {

  if (argc != 3) {
    std::cout << "usage: " << argv[0] << " "
              << "<config-file><measurement-Name>" << std::endl;
    return 1;
  }

  YAML::Node config = YAML::LoadFile(argv[1]);

  // ----------------------- Consumer------------------------------------------
  auto subsyncPrefix = config["Consumer"]["subsyncPrefix"].as<std::string>();
  auto subPrefixdatamain =
      config["Consumer"]["subPrefixdatamain"].as<std::string>();
  auto nSubscription =
      config["Consumer"]["nSubscription"].as<std::vector<int>>();

  // --------------------------------------------------------------------------

  // ##### MEASUREMENT #####
  std::string measurementFileName = argv[2];
  auto measurementConfig = config["Measurement"];
  std::string nodeName = measurementConfig["nodeName"].as<std::string>();
  int saveInterval = measurementConfig["saveInterval"].as<int>();
  ::signal(SIGINT, signalCallbackHandler);
  msCmp = new iceflow::Measurement(measurementFileName, nodeName, saveInterval,
                                   "A");

  try {
    DataFlow(subsyncPrefix, subPrefixdatamain, nSubscription);
  }

  catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}
