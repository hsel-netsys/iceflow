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
#include "iceflow/dag-parser.hpp"
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

NDN_LOG_INIT(iceflow.examples.visionprocessing.aggregate);

iceflow::Measurement *measurementHandler;

void signalCallbackHandler(int signum) {
  measurementHandler->recordToFile();
  std::exit(signum);
}

class Aggregate {
public:
  void aggregate(std::vector<uint8_t> &data) {
    measurementHandler->setField(std::to_string(computeCounter), "CMP_START",
                                 0);

    nlohmann::json deserializedData = Serde::deserialize(data);

    if (deserializedData.contains("frameID")) {
      int frameID = deserializedData["frameID"];
      auto &dataTuple = frameData[frameID];

      if (deserializedData.contains("Age")) {
        std::get<0>(dataTuple) = deserializedData["Age"].get<std::string>();
      }
      if (deserializedData.contains("Gender")) {
        std::get<1>(dataTuple) = deserializedData["Gender"].get<std::string>();
      }
      if (deserializedData.contains("PeopleCount")) {
        std::get<2>(dataTuple) =
            deserializedData["PeopleCount"].get<std::string>();
      }

      // Display the aggregated result
      printAggregateResult(frameID, dataTuple);

      measurementHandler->setField(std::to_string(computeCounter), "CMP_FINISH",
                                   0);
      computeCounter++;
    }
  }

  void printAggregateResult(
      int frameID,
      const std::tuple<std::string, std::string, std::string> &dataTuple) {
    std::string age =
        std::get<0>(dataTuple).empty() ? "N/A" : std::get<0>(dataTuple);
    std::string gender =
        std::get<1>(dataTuple).empty() ? "N/A" : std::get<1>(dataTuple);
    std::string peopleCount =
        std::get<2>(dataTuple).empty() ? "N/A" : std::get<2>(dataTuple);

    std::cout << "Frame ID: " << frameID << "\n"
              << "  Age: " << age << "\n"
              << "  Gender: " << gender << "\n"
              << "  PeopleCount: " << peopleCount << "\n"
              << "------------------------------------" << std::endl;
  }

private:
  std::map<int, std::tuple<std::string, std::string, std::string>> frameData;
  int computeCounter = 0;
};

void run(const std::string &nodeName, const std::string &dagFileName) {

  Aggregate aggregator;
  ndn::Face face;

  auto iceflow =
      std::make_shared<iceflow::IceFlow>(dagFileName, nodeName, face);
  std::vector<std::string> upStreamedges;
  for (int i = 0; i < 3; i++) {
    upStreamedges.push_back(iceflow->getUpstreamEdge(i).value().id);
  }

  auto dagParser = iceflow::DAGParser::parseFromFile(dagFileName);

  auto node = dagParser.findNodeByName(nodeName);

  auto applicationConfiguration = node.applicationConfiguration;

  auto saveThreshold =
      applicationConfiguration.at("measurementsSaveThreshold").get<uint64_t>();

  ::signal(SIGINT, signalCallbackHandler);
  measurementHandler = new iceflow::Measurement(
      nodeName, iceflow->getNodePrefix(), saveThreshold, "A");

  for (auto upstream : upStreamedges) {
    iceflow->registerConsumerCallback(upstream,
                                      [&aggregator](std::vector<uint8_t> data) {
                                        aggregator.aggregate(data);
                                      });
    iceflow->repartitionConsumer(upstream, {0});
  }

  iceflow->run();
}

int main(int argc, const char *argv[]) {

  if (argc != 2) {
    std::string command = argv[0];
    std::cout << "usage: " << command << " <application-dag-file>" << std::endl;
    return 1;
  }

  try {
    std::string nodeName = "aggregate";
    std::string dagFileName = argv[1];

    run(nodeName, dagFileName);
  }

  catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}
