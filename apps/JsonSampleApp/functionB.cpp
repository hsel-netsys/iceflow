#include "iceflow/Consumer.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <yaml-cpp/yaml.h>

using namespace std;

class Compute {
public:
  void compute(std::function<std::string()> receive) {
    while (true) {
      receive();
    }
  }
};

void DataFlow(const std::string &sub_syncPrefix,
              const std::string &userPrefix_data_main,
              const std::vector<int> &nDataStreams) {
  Compute compute;
  ndn::Face interFace;
  iceflow::Consumer consumer(sub_syncPrefix, userPrefix_data_main, nDataStreams,
                             interFace);

  std::vector<std::thread> ConsumerThreads;
  ConsumerThreads.emplace_back(&iceflow::Consumer::run, &consumer);
  ConsumerThreads.emplace_back([&compute, &consumer]() {
    compute.compute(
        [&consumer]() -> std::string { return consumer.receive(); });
  });

  for (auto &t : ConsumerThreads) {
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
  auto sub_syncPrefix = config["Consumer"]["sub_syncPrefix"].as<string>();
  auto sub_Prefix_data_main =
      config["Consumer"]["sub_Prefix_data_main"].as<string>();
  auto nDataStreams = config["Consumer"]["nSub"].as<std::vector<int>>();

  // ----------------------- Producer -----------------------------------------

  //  auto pub_syncPrefix = config["Producer"]["pub_syncPrefix"].as<string>();
  //  auto userPrefix_data_main =
  //      config["Producer"]["userPrefix_data_main"].as<string>();
  //  auto userPrefix_data_manifest =
  //      config["Producer"]["userPrefix_data_manifest"].as<string>();
  //  auto userPrefix_ack = config["Producer"]["userPrefix_ack"].as<string>();
  //  int nPub = config["Producer"]["nDataStreams"].as<int>();
  //  int publish_interval = config["Producer"]["publish_interval"].as<int>();
  //  int publish_interval_new =
  //      config["Producer"]["publish_interval_new"].as<int>();
  //  int output_threshold = config["Producer"]["output_threshold"].as<int>();
  //  int names_in_manifest = config["Producer"]["names_in_manifest"].as<int>();
  //  int map_threshold = config["Producer"]["map_threshold"].as<int>();

  // --------------------------------------------------------------------------

  try {
    DataFlow(sub_syncPrefix, sub_Prefix_data_main, nDataStreams);
  }

  catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}