#include "iceflow/Consumer.hpp"
#include "iceflow/Producer.hpp"
#include <iostream>
#include <ndn-cxx/face.hpp>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <yaml-cpp/yaml.h>

class Compute {
public:
  void lines2words(std::function<std::string()> receive,
                   std::function<void(std::string)> push) {
    while (true) {
      auto line = receive();
      // std::cout<<"Lines: "<<line<<std::endl;
      std::stringstream streamedlines(line);
      std::string word;
      while (streamedlines >> word) {
        push(word);
      }
    }
  }
};

void DataFlow(const std::string &sub_syncPrefix,
              const std::string &sub_Prefix_data_main,
              const std::vector<int> &nDataStreams,
              const std::string &pub_syncPrefix,
              const std::string &pub_Prefix_data_main,
              const std::vector<int> nPub) {
  Compute compute;
  ndn::Face consumerInterFace;
  ndn::Face producerInterFace;
  iceflow::Consumer consumer(sub_syncPrefix, sub_Prefix_data_main, nDataStreams,
                             consumerInterFace);

  iceflow::Producer producer(pub_syncPrefix, pub_Prefix_data_main, nDataStreams,
                             producerInterFace);
  std::vector<std::thread> ProConThreads;
  ProConThreads.emplace_back(&iceflow::Consumer::run, &consumer);
  ProConThreads.emplace_back(&iceflow::Producer::run, &producer);
  ProConThreads.emplace_back([&compute, &consumer, &producer]() {
    compute.lines2words(
        [&consumer]() -> std::string { return consumer.receive(); },
        [&producer](std::string data) { producer.push(data); });
  });

  for (auto &t : ProConThreads) {
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

  // ----------------------- Producer -----------------------------------------

  auto pubsyncPrefix = config["Producer"]["pubsyncPrefix"].as<std::string>();
  auto pubPrefixdatamain =
      config["Producer"]["pubPrefixdatamain"].as<std::string>();
  auto nPartition = config["Producer"]["nPartition"].as<std::vector<int>>();

  // --------------------------------------------------------------------------

  try {
    DataFlow(subsyncPrefix, subPrefixdatamain, nSubscription, pubsyncPrefix,
             pubPrefixdatamain, nPartition);
  }

  catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}