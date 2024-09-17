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
#include "iceflow/producer.hpp"
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
#include <yaml-cpp/yaml.h>

#include <opencv2/dnn.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>

NDN_LOG_INIT(iceflow.examples.visionprocessing.ageDetection);

iceflow::Measurement *measurementHandler;

void signalCallbackHandler(int signum) {
  measurementHandler->recordToFile();
  exit(signum);
}

class AgeDetection {
public:
  void ageDetection(std::function<std::vector<uint8_t>()> receive,
                    std::function<void(const std::vector<uint8_t>)> push,
                    std::string protobufFile, std::string mlModel) {

    cv::Scalar MODEL_MEAN_VALUES =
        cv::Scalar(78.4263377603, 87.7689143744, 114.895847746);
    std::vector<std::string> ageList = {"(0-2)",   "(4-6)",   "(8-12)",
                                        "(15-20)", "(25-32)", "(38-43)",
                                        "(48-53)", "(60-100)"};
    cv::dnn::Net ageNet = cv::dnn::readNet(mlModel, protobufFile);

    int computeCounter = 0;

    while (true) {
      auto encodedcroppedFace = receive();
      nlohmann::json deserializedData = Serde::deserialize(encodedcroppedFace);
      int frameID = deserializedData["frameID"];
      if (deserializedData["image"].empty()) {
        std::cerr << "Received empty Image, cannot decode." << std::endl;
        continue;
      }

      else {
        // Extract frameID and encoded Cropped Face from the deserialized JSON

        std::vector<uint8_t> croppedFace =
            deserializedData["image"].get_binary();

        measurementHandler->setField(std::to_string(computeCounter),
                                     "CMP_START", 0);

        cv::Mat actualFace = cv::imdecode(croppedFace, cv::IMREAD_COLOR);
        // NDN_LOG_INFO("Received: " << actualFace.size() << " bytes");

        measurementHandler->setField(std::to_string(computeCounter), "FD->AD",
                                     0);
        cv::Mat blob = cv::dnn::blobFromImage(actualFace, 1, cv::Size(227, 227),
                                              MODEL_MEAN_VALUES, false);

        ageNet.setInput(blob);
        std::vector<float> agePreds = ageNet.forward();
        // finding maximum indiced in the agePreds vector
        int maxIndiceAge = std::distance(
            agePreds.begin(), max_element(agePreds.begin(), agePreds.end()));
        std::string ageAnalytics = ageList[maxIndiceAge];

        nlohmann::json resultData;
        resultData["frameID"] = frameID;
        resultData["Age"] = ageAnalytics;

        std::vector<uint8_t> encodedAnalytics = Serde::serialize(resultData);

        std::cout << "AgeDetection:\n"
                  << " FrameID:" << frameID
                  << "\n"
                     "Age: "
                  << ageAnalytics << "\n"
                  << " Encoded Image Size: " << encodedAnalytics.size()
                  << " bytes" << std::endl;
        std::cout << "------------------------------------" << std::endl;
        push(encodedAnalytics);
      }

      // ##### MEASUREMENT #####
      measurementHandler->setField(std::to_string(computeCounter), "AD->AGG",
                                   0);
      measurementHandler->setField(std::to_string(computeCounter), "CMP_FINISH",
                                   0);
      computeCounter++;
    }
  }
};

void run(const std::string &syncPrefix, const std::string &nodePrefix,
         const std::string &subTopic, const std::string &pubTopic,
         uint32_t numberOfProducerPartitions,
         std::vector<uint32_t> consumerPartitions,
         std::chrono::milliseconds publishInterval,
         const std::string &protobufFile, const std::string &mlModel) {

  AgeDetection ageDetection;
  ndn::Face face;
  auto iceflow =
      std::make_shared<iceflow::IceFlow>(syncPrefix, nodePrefix, face);

  auto producer = iceflow::IceflowProducer(
      iceflow, pubTopic, numberOfProducerPartitions, publishInterval);

  auto consumer =
      iceflow::IceflowConsumer(iceflow, subTopic, consumerPartitions);

  std::vector<std::thread> threads;
  threads.emplace_back(&iceflow::IceFlow::run, iceflow);
  threads.emplace_back(
      [&ageDetection, &consumer, &producer, &protobufFile, &mlModel]() {
        ageDetection.ageDetection(
            [&consumer]() {
              std::vector<uint8_t> encodedImage = consumer.receiveData();
              return encodedImage;
            },
            [&producer](const std::vector<uint8_t> &analytics) {
              producer.pushData(analytics);
            },
            protobufFile, mlModel);
      });

  for (auto &thread : threads) {
    thread.join();
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
  std::string protobufFile = argv[3];
  std::string mlModel = argv[4];

  YAML::Node config = YAML::LoadFile(configFileName);
  YAML::Node consumerConfig = config["consumer"];
  YAML::Node producerConfig = config["producer"];
  YAML::Node measurementConfig = config["measurements"];

  std::string syncPrefix = config["syncPrefix"].as<std::string>();
  std::string nodePrefix = config["nodePrefix"].as<std::string>();
  std::string pubTopic = producerConfig["topic"].as<std::string>();
  std::string subTopic = consumerConfig["topic"].as<std::string>();
  auto consumerPartitions =
      consumerConfig["partitions"].as<std::vector<uint32_t>>();
  auto numberOfProducerPartitions =
      producerConfig["numberOfPartitions"].as<uint32_t>();
  uint64_t publishInterval = producerConfig["publishInterval"].as<uint64_t>();
  uint64_t saveThreshold = measurementConfig["saveThreshold"].as<uint64_t>();

  ::signal(SIGINT, signalCallbackHandler);
  measurementHandler = new iceflow::Measurement(measurementFileName, nodePrefix,
                                                saveThreshold, "A");

  try {
    run(syncPrefix, nodePrefix, subTopic, pubTopic, numberOfProducerPartitions,
        consumerPartitions, std::chrono::milliseconds(publishInterval),
        protobufFile, mlModel);
  }

  catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}
