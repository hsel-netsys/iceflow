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
#include "iceflow/producer-tlv.hpp"

// ###### MEASUREMENT ######
iceflow::Measurement *msCmp;

void signalCallbackHandler(int signum) {
  msCmp->recordToFile();
  // Terminate program
  exit(signum);
}

class GenderDetector {

public:
  cv::Scalar MODEL_MEAN_VALUES =
      cv::Scalar(78.4263377603, 87.7689143744, 114.895847746);
  std::vector<std::string> genderList = {"Male", "Female"};

  [[noreturn]] void compute(iceflow::RingBuffer<iceflow::Block> *input,
                            iceflow::RingBuffer<iceflow::Block> *output,
                            int outputThreshold, std::string ml_proto,
                            std::string ml_model) {

    cv::dnn::Net genderNet = cv::dnn::readNet(ml_model, ml_proto);

    int computeCounter = 0;

    while (true) {

      auto inputData = input->waitAndPopValue();
      auto frameData = inputData.pullFrame();
      auto jsonData = inputData.pullJson();

      nlohmann::json jsonInput = jsonData.getJson();
      // ##### MEASUREMENT #####
      msCmp->setField(std::to_string(jsonInput["frameID"].get<int>()), "FD->GD",
                      0);
      msCmp->setField(std::to_string(computeCounter), "CMP_START", 0);

      cv::Mat blob;
      blob = cv::dnn::blobFromImage(frameData, 1, cv::Size(227, 227),
                                    MODEL_MEAN_VALUES, false);
      genderNet.setInput(blob);
      std::vector<float> gendPreds = genderNet.forward();

      // finding maximum indiced in the genPreds std::vector
      int maxIndiceGender = std::distance(
          gendPreds.begin(), max_element(gendPreds.begin(), gendPreds.end()));
      std::string gender = genderList[maxIndiceGender];
      NDN_LOG_INFO("gender: " << gender);

      jsonInput["Gender"] = gender;
      m_jsonOutput.setJson(jsonInput);
      NDN_LOG_INFO("Renewed JSON: " << m_jsonOutput.getJson());

      iceflow::Block resultBlock;
      resultBlock.pushJson(m_jsonOutput);

      output->pushData(resultBlock, outputThreshold);
      // ##### MEASUREMENT #####
      msCmp->setField(std::to_string(jsonInput["frameID"].get<int>()),
                      "GD->AGG", 0);
      computeCounter++;
    }
  }

private:
  iceflow::JsonData m_jsonOutput;
};

[[noreturn]] void
fusion(std::vector<iceflow::RingBuffer<iceflow::Block> *> *inputs,
       iceflow::RingBuffer<iceflow::Block> *totalInput, int inputThreshold) {
  while (true) {
    if (!inputs->empty() && totalInput->size() < inputThreshold) {
      for (auto &input : *inputs) {
        auto frameFg = input->waitAndPopValue();
        frameFg.printInfo();
        totalInput->push(frameFg);
      }
    }
  }
}

void DataFlow(std::string &subSyncPrefix, std::vector<int> sub,
              std::string &subPrefixDataMain, std::string &subPrefixAck,
              int inputThreshold, std::string &pubSyncPrefix,
              std::string &userPrefixDataMain,
              const std::string &userPrefixDataManifest,
              const std::string &userPrefixAck, int nDataStreams,
              int publishInterval, int publishIntervalNew, int namesInManifest,
<<<<<<< HEAD
              int outputThreshold, int mapThreshold, const std::string ml_proto,
              const std::string ml_model) {
=======
              int outputThreshold, int mapThreshold,
              const std::string &ml_proto, const std::string &ml_model) {
>>>>>>> 3341744db7299b6138740deb3435b902af42b54c
  std::vector<iceflow::RingBuffer<iceflow::Block> *> inputs;
  iceflow::RingBuffer<iceflow::Block> totalInput;
  // Data
  auto *simpleProducer = new iceflow::ProducerTlv(
      pubSyncPrefix, userPrefixDataMain, userPrefixDataManifest, userPrefixAck,
      nDataStreams, publishInterval, publishIntervalNew, mapThreshold);

  auto *simpleConsumer = new iceflow::ConsumerTlv(
      subSyncPrefix, subPrefixDataMain, subPrefixAck, sub, inputThreshold);

  auto *compute = new GenderDetector();

  inputs.push_back(simpleConsumer->getInputBlockQueue());

  // Data
  std::thread th1(&iceflow::ConsumerTlv::runCon, simpleConsumer);

  std::thread th2(&fusion, &inputs, &totalInput, inputThreshold);

  std::thread th3(&GenderDetector::compute, compute, &totalInput,
                  &simpleProducer->outputQueueBlock, outputThreshold,
                  std::ref(ml_proto), std::ref(ml_model));

  // Data
  std::thread th4(&iceflow::ProducerTlv::runPro, simpleProducer);

  std::vector<std::thread> ProducerThreads;
  ProducerThreads.push_back(std::move(th1));
  NDN_LOG_INFO("Thread " << ProducerThreads.size() << " Started");
  ProducerThreads.push_back(std::move(th2));
  NDN_LOG_INFO("Thread " << ProducerThreads.size() << " Started");
  ProducerThreads.push_back(std::move(th3));
  NDN_LOG_INFO("Thread " << ProducerThreads.size() << " Started");
  ProducerThreads.push_back(std::move(th4));
  NDN_LOG_INFO("Thread " << ProducerThreads.size() << " Started");

  for (auto &t : ProducerThreads) {
    t.join();
  }
}

int main(int argc, char *argv[]) {

  if (argc != 5) {
    std::cout << "usage: " << argv[0] << " "
              << "<config-file><test-name><protobuf_binary><ML-Model>"
              << std::endl;
    return 1;
  }

  YAML::Node config = YAML::LoadFile(argv[1]);

  // ----------------------- Consumer------------------------------------------
  auto subSyncPrefix = config["Consumer"]["subSyncPrefix"].as<std::string>();
  auto subPrefixDataMain =
      config["Consumer"]["subPrefixDataMain"].as<std::string>();
  auto subPrefixDataManifest =
      config["Consumer"]["subPrefixDataManifest"].as<std::string>();
  auto subPrefixAck = config["Consumer"]["subPrefixAck"].as<std::string>();
  auto nSub = config["Consumer"]["nSub"].as<std::vector<int>>();
  int inputThreshold = config["Consumer"]["inputThreshold"].as<int>();

  // ----------------------- Producer -----------------------------------------

  auto pubSyncPrefix = config["Producer"]["pubSyncPrefix"].as<std::string>();
  auto userPrefixDataMain =
      config["Producer"]["userPrefixDataMain"].as<std::string>();
  auto userPrefixDataManifest =
      config["Producer"]["userPrefixDataManifest"].as<std::string>();
  auto userPrefixAck = config["Producer"]["userPrefixAck"].as<std::string>();
  int nDataStreams = config["Producer"]["nDataStreams"].as<int>();
  int publishInterval = config["Producer"]["publishInterval"].as<int>();
  int publishIntervalNew = config["Producer"]["publishIntervalNew"].as<int>();
  int outputThreshold = config["Producer"]["outputThreshold"].as<int>();
  int namesInManifest = config["Producer"]["namesInManifest"].as<int>();
  int mapThreshold = config["Producer"]["mapThreshold"].as<int>();
  std::string ml_proto = argv[3];
  std::string ml_model = argv[4];
  // --------------------------------------------------------------------------

  // ##### MEASUREMENT #####

  std::string nodeName = config["Measurement"]["nodeName"].as<std::string>();
  int saveInterval = config["Measurement"]["saveInterval"].as<int>();
  std::string measurementName = argv[2];
  ::signal(SIGINT, signalCallbackHandler);
  msCmp =
      new iceflow::Measurement(measurementName, nodeName, saveInterval, "A");

  try {
    DataFlow(subSyncPrefix, nSub, subPrefixDataMain, subPrefixAck,
             inputThreshold, pubSyncPrefix, userPrefixDataMain,
             userPrefixDataManifest, userPrefixAck, nDataStreams,
             publishInterval, publishIntervalNew, namesInManifest,
             outputThreshold, mapThreshold, ml_proto, ml_model);
  }

  catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}
