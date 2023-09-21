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
#include <mutex>
#include <thread>

#include "yaml-cpp/yaml.h"

#include "iceflow/consumer-tlv.hpp"
#include "iceflow/measurements.hpp"
#include "iceflow/producer-tlv.hpp"

#include "util.hpp"

// ###### MEASUREMENT ######

iceflow::Measurement *msCmp;

void signalCallbackHandler(int signum) {
  msCmp->recordToFile();
  // Terminate program
  exit(signum);
}

class AgeDetector {

public:
  cv::Scalar MODEL_MEAN_VALUES =
      cv::Scalar(78.4263377603, 87.7689143744, 114.895847746);
  std::vector<std::string> ageList = {"(0-2)",   "(4-6)",   "(8-12)",
                                      "(15-20)", "(25-32)", "(38-43)",
                                      "(48-53)", "(60-100)"};

  [[noreturn]] void compute(iceflow::RingBuffer<iceflow::Block> *input,
                            iceflow::RingBuffer<iceflow::Block> *output,
                            int outputThreshold,
                            std::string protobufBinaryFileName,
                            std::string mlModelFileName) {

    cv::dnn::Net ageNet =
        cv::dnn::readNet(mlModelFileName, protobufBinaryFileName);

    int computeCounter = 0;
    while (true) {
      NDN_LOG_DEBUG("Output size" << output->size());
      NDN_LOG_DEBUG("Input size" << input->size());
      auto start = std::chrono::system_clock::now();

      auto inputData = input->waitAndPopValue();
      auto frameData = pullFrame(inputData);
      auto jsonData = inputData.pullJson();
      nlohmann::json jsonInput = jsonData.getJson();

      // ##### MEASUREMENT #####
      msCmp->setField(std::to_string(jsonInput["frameID"].get<int>()), "FD->AD",
                      0);

      msCmp->setField(std::to_string(computeCounter), "CMP_START", 0);
      if (!frameData.empty()) {
        cv::Mat blob;
        blob = cv::dnn::blobFromImage(frameData, 1, cv::Size(227, 227),
                                      MODEL_MEAN_VALUES, false);
        ageNet.setInput(blob);
        std::vector<float> agePreds = ageNet.forward();
        // finding maximum indiced in the agePreds vector
        int maxIndiceAge = std::distance(
            agePreds.begin(), max_element(agePreds.begin(), agePreds.end()));
        std::string age = ageList[maxIndiceAge];
        NDN_LOG_INFO("Age: " << age);

        jsonInput["Age"] = age;

        m_jsonOutput.setJson(jsonInput);
        NDN_LOG_DEBUG("Renewed JSON: " << m_jsonOutput.getJson());
        iceflow::Block resultBlock;
        resultBlock.pushJson(m_jsonOutput);

        msCmp->setField(std::to_string(computeCounter), "CMP_FINISH", 0);

        auto end = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsedTime = (end - start);
        NDN_LOG_INFO("AD Compute Time: " << elapsedTime.count());

        auto blockingTimeStart = std::chrono::system_clock::now();
        output->pushData(resultBlock, outputThreshold);
        NDN_LOG_DEBUG("Output Queue Size: " << output->size());
        auto blockingTimeEnd = std::chrono::system_clock::now();

        std::chrono::duration<double> elapsedBlockingTime =
            (blockingTimeEnd - blockingTimeStart);
        NDN_LOG_INFO("Blocking Time: " << elapsedBlockingTime.count());
        NDN_LOG_INFO("Absolute Compute time: "
                     << (elapsedTime - elapsedBlockingTime).count());

        // ##### MEASUREMENT #####
        msCmp->setField(std::to_string(jsonInput["frameID"].get<int>()),
                        "AD->AGG", 0);
        computeCounter++;
      }
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
        totalInput->push(frameFg);
      }
    }
  }
}

void startProcessing(std::string &subSyncPrefix, std::vector<int> sub,
                     std::string &subPrefixDataMain, std::string &subPrefixAck,
                     int inputThreshold, std::string &pubSyncPrefix,
                     std::string &userPrefixDataMain,
                     const std::string &userPrefixDataManifest,
                     const std::string &userPrefixAck, int nDataStreams,
                     int publishInterval, int publishIntervalNew,
                     int namesInManifest, int outputThreshold, int mapThreshold,
                     const std::string &protobufBinaryFileName,
                     const std::string &mlModelFileName) {

  std::vector<iceflow::RingBuffer<iceflow::Block> *> inputs;
  iceflow::RingBuffer<iceflow::Block> totalInput;
  // Data
  auto *simpleProducer = new iceflow::ProducerTlv(
      pubSyncPrefix, userPrefixDataMain, userPrefixDataManifest, userPrefixAck,
      nDataStreams, publishInterval, publishIntervalNew, mapThreshold);

  auto *simpleConsumer = new iceflow::ConsumerTlv(
      subSyncPrefix, subPrefixDataMain, subPrefixAck, sub, inputThreshold);

  auto *compute = new AgeDetector();

  inputs.push_back(simpleConsumer->getInputBlockQueue());

  // Data
  std::vector<std::thread> threads;
  threads.emplace_back(&iceflow::ConsumerTlv::runCon, simpleConsumer);
  threads.emplace_back(&fusion, &inputs, &totalInput, inputThreshold);
  threads.emplace_back(&AgeDetector::compute, compute, &totalInput,
                       &simpleProducer->outputQueueBlock, outputThreshold,
                       std::ref(protobufBinaryFileName),
                       std::ref(mlModelFileName));
  threads.emplace_back(&iceflow::ProducerTlv::runPro, simpleProducer);

  int threadCounter = 0;
  for (auto &thread : threads) {
    thread.join();
    NDN_LOG_INFO("Thread " << threadCounter++ << " started");
  }
}

int main(int argc, const char *argv[]) {

  if (argc != 5) {
    std::cout << "usage: " << argv[0] << " "
              << "<config-file><test-name><protobuf_binary><ML-Model>"
              << std::endl;
    return 1;
  }

  std::string configFileName = argv[1];
  std::string measurementFileName = argv[2];
  std::string protobufBinaryFileName = argv[3];
  std::string mlModelFileName = argv[4];

  YAML::Node config = YAML::LoadFile(configFileName);
  auto consumerConfig = config["Consumer"];
  auto producerConfig = config["Producer"];
  auto measurementConfig = config["Measurement"];

  // ----------------------- Consumer------------------------------------------
  auto subSyncPrefix = consumerConfig["subSyncPrefix"].as<std::string>();
  auto subPrefixDataMain =
      consumerConfig["subPrefixDataMain"].as<std::string>();
  auto subPrefixDataManifest =
      consumerConfig["subPrefixDataManifest"].as<std::string>();
  auto subPrefixAck = consumerConfig["subPrefixAck"].as<std::string>();
  auto nSub = consumerConfig["nSub"].as<std::vector<int>>();
  int inputThreshold = consumerConfig["inputThreshold"].as<int>();

  // ----------------------- Producer -----------------------------------------

  auto pubSyncPrefix = producerConfig["pubSyncPrefix"].as<std::string>();
  auto userPrefixDataMain =
      producerConfig["userPrefixDataMain"].as<std::string>();
  auto userPrefixDataManifest =
      producerConfig["userPrefixDataManifest"].as<std::string>();
  auto userPrefixAck = producerConfig["userPrefixAck"].as<std::string>();
  int nDataStreams = producerConfig["nDataStreams"].as<int>();
  int publishInterval = producerConfig["publishInterval"].as<int>();
  int publishIntervalNew = producerConfig["publishIntervalNew"].as<int>();
  int outputThreshold = producerConfig["outputThreshold"].as<int>();
  int namesInManifest = producerConfig["namesInManifest"].as<int>();
  int mapThreshold = producerConfig["mapThreshold"].as<int>();

  // --------------------------------------------------------------------------

  // ##### MEASUREMENT #####

  std::string nodeName = measurementConfig["nodeName"].as<std::string>();
  int saveInterval = measurementConfig["saveInterval"].as<int>();

  ::signal(SIGINT, signalCallbackHandler);
  msCmp = new iceflow::Measurement(measurementFileName, nodeName, saveInterval,
                                   "A");

  try {
    startProcessing(subSyncPrefix, nSub, subPrefixDataMain, subPrefixAck,
                    inputThreshold, pubSyncPrefix, userPrefixDataMain,
                    userPrefixDataManifest, userPrefixAck, nDataStreams,
                    publishInterval, publishIntervalNew, namesInManifest,
                    outputThreshold, mapThreshold, protobufBinaryFileName,
                    mlModelFileName);
  }

  catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}
