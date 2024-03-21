#include "iceflow/Producer.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <yaml-cpp/yaml.h>

class Compute {
public:
  void compute(std::function<void(std::string)> push) {
    int i = 1;
    while (i < 5000) {
      std::string data = std::to_string(i) + "Hello ";
      std::cout << data << std::endl;
      i++;
      push(data);
    }
  }
};

void DataFlow(const std::string &pub_syncPrefix,
              const std::string &userPrefix_data_main,
              const std::vector<int> &nDataStreams) {
  Compute compute;
  ndn::Face interFace;
  iceflow::Producer producer(pub_syncPrefix, userPrefix_data_main, nDataStreams,
                             interFace);

  std::vector<std::thread> ProducerThreads;
  ProducerThreads.emplace_back(&iceflow::Producer::run, &producer);
  ProducerThreads.emplace_back([&compute, &producer]() {
    compute.compute([&producer](std::string data) { producer.push(data); });
  });

  for (auto &t : ProducerThreads) {
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

  auto pub_syncPrefix = config["Producer"]["pub_syncPrefix"].as<std::string>();
  auto userPrefix_data_main =
      config["Producer"]["userPrefix_data_main"].as<std::string>();
  auto nDataStreams = config["Producer"]["nDataStreams"].as<std::vector<int>>();

  try {
    DataFlow(pub_syncPrefix, userPrefix_data_main, nDataStreams);
  } catch (const std::exception &e) {
    std::cout << e.what() << std::endl;
  }

  return 0;
}
