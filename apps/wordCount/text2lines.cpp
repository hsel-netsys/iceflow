#include "iceflow/Producer.hpp"
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#include <yaml-cpp/yaml.h>

class Compute {
public:
  void compute(const std::string &filename,
               std::function<void(std::string)> push) {
    std::ifstream file;
    file.open(filename);

    if (file.is_open()) {
      for (std::string line; getline(file, line);) {
        //				std::cout << line + ";" << std::endl;
        push(line);
      }
      file.close();
    } else {
      std::cerr << "Error opening file: " << filename << std::endl;
    }
  }
};

void DataFlow(const std::string &pub_syncPrefix,
              const std::string &userPrefix_data_main,
              std::vector<int> &nDataStreams, const std::string &filename) {
  Compute compute;
  ndn::Face interFace;

  std::cout << "Starting IceFlow Stream Processing - - - - 2 " << pub_syncPrefix
            << " " << userPrefix_data_main << " " << nDataStreams.size() << " "
            << filename + " " << std::endl;
  iceflow::Producer producer(pub_syncPrefix, userPrefix_data_main, nDataStreams,
                             interFace);
  std::cout << "Starting IceFlow Stream Processing - - - - 3" << std::endl;
  std::vector<std::thread> ProducerThreads;
  ProducerThreads.emplace_back(&iceflow::Producer::run, &producer);
  ProducerThreads.emplace_back([&compute, &producer, &filename]() {
    compute.compute(filename,
                    [&producer](std::string data) { producer.push(data); });
  });

  for (auto &t : ProducerThreads) {
    t.join();
  }
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cout << "usage: " << argv[0] << " "
              << "<config-file><text-file>" << std::endl;
    return 1;
  }

  YAML::Node config = YAML::LoadFile(argv[1]);

  auto pub_syncPrefix = config["Producer"]["pub_syncPrefix"].as<std::string>();
  auto userPrefix_data_main =
      config["Producer"]["userPrefix_data_main"].as<std::string>();
  auto nDataStreams = config["Producer"]["nDataStreams"].as<std::vector<int>>();
  std::cout << nDataStreams.size() << std::endl;
  std::string filename = argv[2];

  try {
    std::cout << "Starting IceFlow Stream Processing - - - - 1" << std::endl;
    for (int i = 0; i < nDataStreams.size(); ++i) {
      std::cout << "Streams: " << nDataStreams[i] << std::endl;
    }
    DataFlow(pub_syncPrefix, userPrefix_data_main, nDataStreams, filename);
  } catch (const std::exception &e) {
    std::cout << e.what() << std::endl;
  }

  return 0;
}
