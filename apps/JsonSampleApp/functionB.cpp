//
// Created by Laura Alwardani on 25.08.22.
//
#include "core/consumer_psync.hpp"
#include "core/consumer_tlv.hpp"
#include "core/producer_psync.hpp"
#include "core/producer_tlv.hpp"

#include "yaml-cpp/yaml.h"
#include <thread>

#include <iostream>
#include <iterator>
#include <tuple>

using namespace std;

class Compute {

public:
  [[noreturn]] void compute(TSQueue<IceFlowBlock> *input,
                            TSQueue<IceFlowBlock> *output,
                            int output_threshold) {

    while (true) {
      auto input_data = input->waitAndPopValue();
      auto json_data = input_data.pullJson();
      cout << "input data: " << json_data.getJson() << endl;
      usleep(100000);

      IceFlowBlock result_block;
      result_block.pushJson(json_data);
      output->pushData(result_block, output_threshold);
    }
  }

private:
};

[[noreturn]] void fusion(vector<TSQueue<IceFlowBlock> *> *inputs,
                         TSQueue<IceFlowBlock> *totalInput) {
  while (!inputs->empty()) {
    for (auto &input : *inputs) {
      auto frame_fg = input->waitAndPopValue();
      totalInput->push(frame_fg);
    }
  }
}

void DataFlow(string &sub_syncPrefix, int sub, string &sub_Prefix_data_main,
              string &sub_Prefix_ack, int input_threshold,
              string &pub_syncPrefix, string &userPrefix_data_main,
              const string &userPrefix_data_manifest,
              const string &userPrefix_ack, int nDataStreams,
              int publish_interval, int publish_interval_new,
              int names_in_manifest, int output_threshold, int map_threshold) {
  vector<TSQueue<IceFlowBlock> *> inputs;
  TSQueue<IceFlowBlock> totalInput;
  // Data
  auto *simple_producer =
      new ProducerTLV(pub_syncPrefix, userPrefix_data_main,
                      userPrefix_data_manifest, userPrefix_ack, nDataStreams,
                      publish_interval, publish_interval_new, map_threshold);

  auto *simple_consumer = new ConsumerTLV(sub_syncPrefix, sub_Prefix_data_main,
                                          sub_Prefix_ack, sub, input_threshold);

  auto *compute = new Compute();

  inputs.push_back(&simple_consumer->input_queue_block);

  std::thread th1(&ConsumerTLV::run_con, simple_consumer);
  std::thread th2(&fusion, &inputs, &totalInput);
  std::thread th3(&Compute::compute, compute, &totalInput,
                  &simple_producer->output_queue_block, output_threshold);
  std::thread th4(&ProducerTLV::run_pro, simple_producer);

  vector<std::thread> ProducerThreads;
  ProducerThreads.push_back(std::move(th1));
  //    cout << "Thread " << ProducerThreads.size() << " Started" << endl;
  ProducerThreads.push_back(std::move(th2));
  //    cout << "Thread " << ProducerThreads.size() << " Started" << endl;
  ProducerThreads.push_back(std::move(th3));
  //    cout << "Thread " << ProducerThreads.size() << " Started" << endl;
  ProducerThreads.push_back(std::move(th4));
  //    cout << "Thread " << ProducerThreads.size() << " Started" << endl;

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

  // ----------------------- Consumer------------------------------------------
  auto sub_syncPrefix = config["Consumer"]["sub_syncPrefix"].as<string>();
  auto sub_Prefix_data_main =
      config["Consumer"]["sub_Prefix_data_main"].as<string>();
  auto sub_Prefix_data_manifest =
      config["Consumer"]["sub_Prefix_data_manifest"].as<string>();
  auto sub_prefix_ack = config["Consumer"]["sub_Prefix_ack"].as<string>();
  int nSub = config["Consumer"]["nSub"].as<int>();
  int input_threshold = config["Consumer"]["input_threshold"].as<int>();

  // ----------------------- Producer -----------------------------------------

  auto pub_syncPrefix = config["Producer"]["pub_syncPrefix"].as<string>();
  auto userPrefix_data_main =
      config["Producer"]["userPrefix_data_main"].as<string>();
  auto userPrefix_data_manifest =
      config["Producer"]["userPrefix_data_manifest"].as<string>();
  auto userPrefix_ack = config["Producer"]["userPrefix_ack"].as<string>();
  int nDataStreams = config["Producer"]["nDataStreams"].as<int>();
  int publish_interval = config["Producer"]["publish_interval"].as<int>();
  int publish_interval_new =
      config["Producer"]["publish_interval_new"].as<int>();
  int output_threshold = config["Producer"]["output_threshold"].as<int>();
  int names_in_manifest = config["Producer"]["names_in_manifest"].as<int>();
  int map_threshold = config["Producer"]["map_threshold"].as<int>();

  // --------------------------------------------------------------------------

  try {
    DataFlow(sub_syncPrefix, nSub, sub_Prefix_data_main, sub_prefix_ack,
             input_threshold, pub_syncPrefix, userPrefix_data_main,
             userPrefix_data_manifest, userPrefix_ack, nDataStreams,
             publish_interval, publish_interval_new, names_in_manifest,
             output_threshold, map_threshold);
  }

  catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}