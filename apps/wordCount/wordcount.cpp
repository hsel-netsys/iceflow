#include "iceflow/Consumer.hpp"
#include <iostream>
#include <ndn-cxx/face.hpp>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <yaml-cpp/yaml.h>

class Compute {
public:
  void countWord(std::function<std::string()> receive) {
    while (true) {
      std::string words = receive();
      std::istringstream stream(words);
      std::string word;
      while (stream >> word) {
        // Convert the word to lowercase to make the counting case-insensitive
        for (char &c : word) {
          c = std::tolower(c);
        }

        // Increment the count for the word in the map
        wordCountMap[word]++;
        printOccurances();
      }
    }
  }
  void printOccurances() {
    std::cout << "Word occurrences:\n";
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

  if (argc != 2) {
    std::cout << "usage: " << argv[0] << " "
              << "<config-file>" << std::endl;
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

  try {
    DataFlow(subsyncPrefix, subPrefixdatamain, nSubscription);
  }

  catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}