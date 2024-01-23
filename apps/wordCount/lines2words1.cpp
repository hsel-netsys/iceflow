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
  void compute(std::function<std::string()> receive,
               std::function<void(std::string)> push) {
    while (true) {
      auto line = receive();
      std::stringstream streamedlines(line);
      std::string word;
      while (streamedlines >> word) {
        //        std::cout << "Word:" << word << std::endl;
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
  //  ndn::Face interFace;
  iceflow::Consumer consumer(sub_syncPrefix, sub_Prefix_data_main, nDataStreams,
                             consumerInterFace);

  iceflow::Producer producer(pub_syncPrefix, pub_Prefix_data_main, nDataStreams,
                             producerInterFace);
  std::vector<std::thread> ProConThreads;
  ProConThreads.emplace_back(&iceflow::Consumer::run, &consumer);
  ProConThreads.emplace_back(&iceflow::Producer::run, &producer);
  ProConThreads.emplace_back([&compute, &consumer, &producer]() {
    compute.compute([&consumer]() -> std::string { return consumer.receive(); },
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
  auto sub_syncPrefix = config["Consumer"]["sub_syncPrefix"].as<std::string>();
  auto sub_Prefix_data_main =
      config["Consumer"]["sub_Prefix_data_main"].as<std::string>();
  auto nDataStreams = config["Consumer"]["nSub"].as<std::vector<int>>();

  // ----------------------- Producer -----------------------------------------

  auto pub_syncPrefix = config["Producer"]["pub_syncPrefix"].as<std::string>();
  auto pub_Prefix_data_main =
      config["Producer"]["pub_Prefix_data_main"].as<std::string>();
  auto nPub = config["Producer"]["nDataStreams"].as<std::vector<int>>();

  // --------------------------------------------------------------------------

  try {
    DataFlow(sub_syncPrefix, sub_Prefix_data_main, nDataStreams, pub_syncPrefix,
             pub_Prefix_data_main, nPub);
  }

  catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}