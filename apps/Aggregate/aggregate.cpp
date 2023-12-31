/*
 * Copyright 2022 The IceFlow Authors.
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

#include <csignal>
#include <thread>

#include "yaml-cpp/yaml.h"

#include "iceflow/consumer-tlv.hpp"
#include "iceflow/measurements.hpp"

#include "util.hpp"

// ###### MEASUREMENT ######

iceflow::Measurement *msCmp;

void signalCallbackHandler(int signum) {
  msCmp->recordToFile();
  // Terminate program
  exit(signum);
}

class Aggregate {

public:
  [[noreturn]] void compute(iceflow::RingBuffer<iceflow::Block> *input) {
    typedef std::multimap<std::string, std::string>::iterator MMAPIterator;
    int computeCounter = 0;

    while (true) {
      auto inputData = input->waitAndPopValue();
      auto jsonData = inputData.pullJson();
      NDN_LOG_INFO("input json: " << jsonData.getJson());

      nlohmann::json jsonInput = jsonData.getJson();

      msCmp->setField(std::to_string(computeCounter), "CMP_START", 0);

      msCmp->setField(std::to_string(jsonInput["frameID"].get<int>()),
                      "IS->AGG", 0);

      if (jsonInput.contains("Age") || jsonInput.contains("Gender") ||
          jsonInput.contains("Emotion") || jsonInput.contains("People")) {
        msCmp->setField(std::to_string(jsonInput["frameID"].get<int>()),
                        "ED->AGG", 0);
        m_frameId.push_back(std::to_string(jsonInput["frameID"].get<int>()));
      }
      if (jsonInput.contains("Age")) {
        msCmp->setField(std::to_string(jsonInput["frameID"].get<int>()),
                        "AD->AGG", 0);
        m_analysisStore.insert(
            std::make_pair(std::to_string(jsonInput["frameID"].get<int>()),
                           jsonInput.at("Age")));
      } else if (jsonInput.contains("Gender")) {
        msCmp->setField(std::to_string(jsonInput["frameID"].get<int>()),
                        "GD->AGG", 0);
        m_analysisStore.insert(
            std::make_pair(std::to_string(jsonInput["frameID"].get<int>()),
                           jsonInput.at("Gender")));
      } else if (jsonInput.contains("Emotion")) {
        msCmp->setField(std::to_string(jsonInput["frameID"].get<int>()),
                        "ED->AGG", 0);
        m_analysisStore.insert(
            std::make_pair(std::to_string(jsonInput["frameID"].get<int>()),
                           jsonInput.at("Emotion")));
      } else if (jsonInput.contains("People")) {
        msCmp->setField(std::to_string(jsonInput["frameID"].get<int>()),
                        "PC->AGG", 0);
        m_analysisStore.insert(
            std::make_pair(std::to_string(jsonInput["frameID"].get<int>()),
                           jsonInput.at("People")));
      }

      for (auto &i : m_frameId) {
        if (m_analysisStore.count(i) > 1) {
          NDN_LOG_INFO("Frame Id: " << i);
          std::pair<MMAPIterator, MMAPIterator> result =
              m_analysisStore.equal_range(i);
          for (auto it = result.first; it != result.second; it++) {
            NDN_LOG_INFO(it->second);
          }
        }
      }
      msCmp->setField(std::to_string(computeCounter), "CMP_FINISH", 0);
      computeCounter++;
    }
  }

private:
  std::vector<std::string> m_frameId;
  std::multimap<std::string, std::string> m_analysisStore;
};

[[noreturn]] void
fusion(std::vector<iceflow::RingBuffer<iceflow::Block> *> *inputs,
       iceflow::RingBuffer<iceflow::Block> *totalInput, int inputThreshold) {
  while (true) {
    if (!inputs->empty() && totalInput->size() < inputThreshold) {
      for (int i = 0; i < inputs->size(); i++) {
        if (inputs->at(i)->size() > 0) {
          auto frameFg = inputs->at(i)->waitAndPopValue();
          totalInput->push(frameFg);
        }
      }
    }
  }
}

void startProcessing(std::string &subSyncPrefix1, std::vector<int> nSub1,
                     std::string &subPrefixDataMain1,
                     std::string &subPrefixAck1, int inputThreshold1,
                     std::string &subSyncPrefix2, std::vector<int> nSub2,
                     std::string &subPrefixDataMain2,
                     std::string &subPrefixAck2, int inputThreshold2,
                     std::string &subSyncPrefix3, std::vector<int> nSub3,
                     std::string &subPrefixDataMain3,
                     std::string &subPrefixAck3, int inputThreshold3,
                     std::string &subSyncPrefix4, std::vector<int> nSub4,
                     std::string &subPrefixDataMain4,
                     std::string &subPrefixAck4, int inputThreshold4) {

  // Data
  auto *simpleConsumer1 =
      new iceflow::ConsumerTlv(subSyncPrefix1, subPrefixDataMain1,
                               subPrefixAck1, nSub1, inputThreshold1);
  auto *simpleConsumer2 =
      new iceflow::ConsumerTlv(subSyncPrefix2, subPrefixDataMain2,
                               subPrefixAck2, nSub2, inputThreshold2);
  auto *simpleConsumer3 =
      new iceflow::ConsumerTlv(subSyncPrefix3, subPrefixDataMain3,
                               subPrefixAck3, nSub3, inputThreshold3);
  auto *simpleConsumer4 =
      new iceflow::ConsumerTlv(subSyncPrefix4, subPrefixDataMain4,
                               subPrefixAck4, nSub4, inputThreshold4);

  auto *compute = new Aggregate();

  std::vector<iceflow::RingBuffer<iceflow::Block> *> inputs;
  iceflow::RingBuffer<iceflow::Block> totalInput;

  inputs.push_back(simpleConsumer1->getInputBlockQueue());
  inputs.push_back(simpleConsumer2->getInputBlockQueue());
  inputs.push_back(simpleConsumer3->getInputBlockQueue());
  inputs.push_back(simpleConsumer4->getInputBlockQueue());

  std::vector<std::thread> threads;
  threads.emplace_back(&iceflow::ConsumerTlv::runCon, simpleConsumer1);
  threads.emplace_back(&iceflow::ConsumerTlv::runCon, simpleConsumer2);
  threads.emplace_back(&iceflow::ConsumerTlv::runCon, simpleConsumer3);
  threads.emplace_back(&iceflow::ConsumerTlv::runCon, simpleConsumer4);
  threads.emplace_back(&fusion, &inputs, &totalInput, inputThreshold1);
  threads.emplace_back(&Aggregate::compute, compute, &totalInput);

  int threadCounter = 0;
  for (auto &thread : threads) {
    NDN_LOG_INFO("Thread " << threadCounter++ << " started");
    thread.join();
  }
}

int main(int argc, const char *argv[]) {

  if (argc != 3) {
    std::cout << "usage: " << argv[0] << " "
              << "<config-file><test-name>" << std::endl;
    return 1;
  }

  std::string configFileName = argv[1];
  std::string measurementFileName = argv[2];

  YAML::Node config = YAML::LoadFile(configFileName);
  auto consumer1Config = config["Consumer1"];
  auto consumer2Config = config["Consumer2"];
  auto consumer3Config = config["Consumer3"];
  auto consumer4Config = config["Consumer4"];
  auto measurementConfig = config["Measurement"];

  //	----------------------- Consumer 1
  //-----------------------------------------
  auto subSyncPrefix1 = consumer1Config["subSyncPrefix"].as<std::string>();
  auto subPrefixDataMain1 =
      consumer1Config["subPrefixDataMain"].as<std::string>();
  auto subPrefixDataManifest1 =
      consumer1Config["subPrefixDataManifest"].as<std::string>();
  auto subPrefixAck1 = consumer1Config["subPrefixAck"].as<std::string>();
  auto nSub1 = consumer1Config["nSub"].as<std::vector<int>>();
  int inputThreshold1 = consumer1Config["inputThreshold"].as<int>();

  //	----------------------------------------------------------------------------

  //	----------------------- Consumer 2
  //-----------------------------------------
  auto subSyncPrefix2 = consumer2Config["subSyncPrefix"].as<std::string>();
  auto subPrefixDataMain2 =
      consumer2Config["subPrefixDataMain"].as<std::string>();
  auto subPrefixDataManifest2 =
      consumer2Config["subPrefixDataManifest"].as<std::string>();
  auto subPrefixAck2 = consumer2Config["subPrefixAck"].as<std::string>();
  auto nSub2 = consumer2Config["nSub"].as<std::vector<int>>();
  int inputThreshold2 = consumer2Config["inputThreshold"].as<int>();

  //	----------------------------------------------------------------------------

  //	----------------------- Consumer 3
  //-----------------------------------------
  auto subSyncPrefix3 = consumer3Config["subSyncPrefix"].as<std::string>();
  auto subPrefixDataMain3 =
      consumer3Config["subPrefixDataMain"].as<std::string>();
  auto subPrefixDataManifest3 =
      consumer3Config["subPrefixDataManifest"].as<std::string>();
  auto subPrefixAck3 = consumer3Config["subPrefixAck"].as<std::string>();
  auto nSub3 = consumer3Config["nSub"].as<std::vector<int>>();
  int inputThreshold3 = consumer3Config["inputThreshold"].as<int>();

  //	----------------------------------------------------------------------------

  //	----------------------- Consumer 4
  //-----------------------------------------
  auto subSyncPrefix4 = consumer4Config["subSyncPrefix"].as<std::string>();
  auto subPrefixDataMain4 =
      consumer4Config["subPrefixDataMain"].as<std::string>();
  auto subPrefixDataManifest4 =
      consumer4Config["subPrefixDataManifest"].as<std::string>();
  auto subPrefixAck4 = consumer4Config["subPrefixAck"].as<std::string>();
  auto nSub4 = consumer4Config["nSub"].as<std::vector<int>>();
  int inputThreshold4 = consumer4Config["inputThreshold"].as<int>();

  //	----------------------------------------------------------------------------

  // ##### MEASUREMENT #####

  std::string nodeName = measurementConfig["nodeName"].as<std::string>();
  int saveInterval = measurementConfig["saveInterval"].as<int>();

  ::signal(SIGINT, signalCallbackHandler);
  msCmp = new iceflow::Measurement(measurementFileName, nodeName, saveInterval,
                                   "A");

  try {
    startProcessing(subSyncPrefix1, nSub1, subPrefixDataMain1, subPrefixAck1,
                    inputThreshold1, subSyncPrefix2, nSub2, subPrefixDataMain2,
                    subPrefixAck2, inputThreshold2, subSyncPrefix3, nSub3,
                    subPrefixDataMain3, subPrefixAck3, inputThreshold3,
                    subSyncPrefix4, nSub4, subPrefixDataMain4, subPrefixAck4,
                    inputThreshold4);
  } catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}
