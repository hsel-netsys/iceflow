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

#include "opencv2/objdetect.hpp"
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

class PeopleCounter {

public:
  [[noreturn]] void compute(iceflow::RingBuffer<iceflow::Block> *input,
                            iceflow::RingBuffer<iceflow::Block> *output,
                            int outputThreshold) {
    int computeCounter = 0;
    cv::HOGDescriptor hog;
    hog.setSVMDetector(cv::HOGDescriptor::getDefaultPeopleDetector());
    while (true) {
      if (input->size() > 0) {

        // To DO - Only Mat needed - provide grey_cam_image
        NDN_LOG_INFO("Compute Input Queue Size: " << input->size());
        auto inputData = input->waitAndPopValue();
        auto start = std::chrono::system_clock::now();
        auto frameData = inputData.pullFrame();
        auto jsonData = inputData.pullJson();
        NDN_LOG_INFO("Input Json: " << jsonData.getJson());

        nlohmann::json jsonInput = jsonData.getJson();
        // ##### MEASUREMENT #####
        msCmp->setField(std::to_string(jsonInput["frameID"].get<int>()),
                        "IS->PC", 0);
        msCmp->setField(std::to_string(computeCounter), "CMP_START", 0);

        // returns the number of people detected from detect and draw function
        int numPeople = detect(hog, frameData);
        NDN_LOG_INFO("People count: " << numPeople);

        jsonInput["People"] = std::to_string(numPeople);
        m_jsonOutput.setJson(jsonInput);
        NDN_LOG_INFO("Renewed JSON: " << m_jsonOutput.getJson());
        msCmp->setField(std::to_string(computeCounter), "CMP_FINISH", 0);
        iceflow::Block resultBlock;
        resultBlock.pushJson(m_jsonOutput);

        // Push the processed result
        auto end = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsedTime = end - start;
        NDN_LOG_INFO("Compute time: " << elapsedTime.count());
        output->pushData(resultBlock, outputThreshold);

        NDN_LOG_INFO("Output Queue Size: " << output->size());

        // ##### MEASUREMENT #####
        msCmp->setField(std::to_string(jsonInput["frameID"].get<int>()),
                        "PC->AGG", 0);
        computeCounter++;
      }
    }
  }

  int detect(const cv::HOGDescriptor &hog, cv::Mat &img) {
    std::vector<cv::Rect> found, foundFiltered;
    hog.detectMultiScale(img, found, 0, cv::Size(8, 8), cv::Size(16, 16), 1.07,
                         2);

    for (size_t i = 0; i < found.size(); i++) {
      cv::Rect r = found[i];
      size_t j;
      // Do not add small detections inside a bigger detection.
      for (j = 0; j < found.size(); j++)
        if (j != i && (r & found[j]) == r)
          break;
      if (j == found.size())
        foundFiltered.push_back(r);
    }
    return foundFiltered.size();
  }

private:
  iceflow::JsonData m_jsonOutput;
};

[[noreturn]] void
fusion(std::vector<iceflow::RingBuffer<iceflow::Block> *> *inputs,
       iceflow::RingBuffer<iceflow::Block> *totalInput, int inputThreshold) {
  while (true) {
    if (!inputs->empty() && totalInput->size() < inputThreshold) {
      for (int i = 0; i < inputs->size(); i++) {
        if (inputs->at(i)->size() > 0) {
          auto frameFg = inputs->at(i)->waitAndPopValue();
          NDN_LOG_INFO("FUSION:  ");
          totalInput->push(frameFg);
        }
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
              int outputThreshold, int mapThreshold, int computeThreads) {
  std::vector<iceflow::RingBuffer<iceflow::Block> *> inputs;
  iceflow::RingBuffer<iceflow::Block> totalInput;
  auto *simpleProducer = new iceflow::ProducerTlv(
      pubSyncPrefix, userPrefixDataMain, userPrefixDataManifest, userPrefixAck,
      nDataStreams, publishInterval, publishIntervalNew, mapThreshold);

  auto *simpleConsumer = new iceflow::ConsumerTlv(
      subSyncPrefix, subPrefixDataMain, subPrefixAck, sub, inputThreshold);

  auto *compute = new PeopleCounter();
  std::vector<std::thread> ThreadCollector;
  inputs.push_back(simpleConsumer->getInputBlockQueue());

  // Data
  std::thread th1(&iceflow::ConsumerTlv::runCon, simpleConsumer);

  std::thread th2(&fusion, &inputs, &totalInput, inputThreshold);
  for (int i = 0; i < computeThreads; ++i) {

    ThreadCollector.emplace_back([&]() {
      compute->compute(&totalInput, &simpleProducer->outputQueueBlock,
                       outputThreshold);
    });
  }

  // Data
  std::thread th3(&iceflow::ProducerTlv::runPro, simpleProducer);

  ThreadCollector.push_back(std::move(th1));
  NDN_LOG_INFO("Thread " << ThreadCollector.size() << " Started");
  ThreadCollector.push_back(std::move(th2));
  NDN_LOG_INFO("Thread " << ThreadCollector.size() << " Started");
  ThreadCollector.push_back(std::move(th3));
  NDN_LOG_INFO("Thread " << ThreadCollector.size() << " Started");

  for (auto &t : ThreadCollector) {
    t.join();
  }
}

int main(int argc, char *argv[]) {

  if (argc != 3) {
    std::cout << "usage: " << argv[0] << " "
              << "<config-file><test-name>" << std::endl;
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
  int computeThreads = config["Producer"]["computeThreads"].as<int>();

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
             outputThreshold, mapThreshold, computeThreads);
  }

  catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}
