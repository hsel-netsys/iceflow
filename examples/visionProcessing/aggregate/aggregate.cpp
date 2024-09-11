/*
 * Copyright 2024 The IceFlow Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "iceflow/consumer.hpp"
#include "iceflow/iceflow.hpp"
#include "iceflow/measurements.hpp"

#include <csignal>
#include <iostream>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/logger.hpp>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <yaml-cpp/yaml.h>

NDN_LOG_INIT(iceflow.examples.visionprocessing.aggregate);
iceflow::Measurement *measurementHandler;

void signalCallbackHandler(int signum) {
  measurementHandler->recordToFile();
  exit(signum);
}

class Aggregate {
public:
  void aggregate(std::function<std::string()> receive1,
                 std::function<std::string()> receive2,
                 std::function<std::string()> receive3) {
    int computeCounter = 0;

    while (true) {
      auto personAnalysis1 = receive1();
      auto personAnalysis2 = receive2();
      auto personAnalysis3 = receive3();

      measurementHandler->setField(std::to_string(computeCounter), "CMP_START",
                                   0);

      std::cout << "Analysis " << personAnalysis1 << std::endl;
      std::cout << personAnalysis2 << std::endl;
      std::cout << personAnalysis3 << std::endl;

      computeCounter++;
    }
  }
};

void run(const std::string &syncPrefix, const std::string &nodePrefix,
         const std::string &subTopic1,
         std::vector<uint32_t> consumerPartitions1,
         const std::string &subTopic2,
         std::vector<uint32_t> consumerPartitions2,
         const std::string &subTopic3,
         std::vector<uint32_t> consumerPartitions3) {

  Aggregate aggregate;
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

  threads.emplace_back([&aggregate, &consumer1, &consumer2, &consumer3]() {
    aggregate.aggregate(
        [&consumer1]() -> std::string {
          auto encodedAnalysis = consumer1.receiveData();
          std::string analysis(encodedAnalysis.begin(), encodedAnalysis.end());
          return analysis;
        },
        [&consumer2]() -> std::string {
          auto encodedAnalysis = consumer2.receiveData();
          std::string analysis(encodedAnalysis.begin(), encodedAnalysis.end());
          return analysis;
        },
        [&consumer3]() -> std::string {
          auto encodedAnalysis = consumer3.receiveData();
          std::string analysis(encodedAnalysis.begin(), encodedAnalysis.end());
          return analysis;
        });
  });

  // Join the threads
  for (auto &thread : threads) {
    thread.join();
  }
}

int main(int argc, const char *argv[]) {

  if (argc != 3) {
    std::cout << "usage: " << argv[0]
              << " <config-file><test-name><measurement-file>" << std::endl;
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
