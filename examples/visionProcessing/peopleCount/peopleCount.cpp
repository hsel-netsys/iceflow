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
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <vector>
#include <yaml-cpp/yaml.h>

#include "opencv2/objdetect.hpp"
#include <opencv2/opencv.hpp>

NDN_LOG_INIT(iceflow.examples.visionprocessing.peopleCounter);

iceflow::Measurement *measurementHandler;

void signalCallbackHandler(int signum) {
  measurementHandler->recordToFile();
  exit(signum);
}

class PeopleCounter {
public:
  void peopleCount(std::function<std::vector<uint8_t>()> receive,
                   std::function<void(const std::vector<uint8_t>)> push) {

    int computeCounter = 0;
    cv::HOGDescriptor hog;
    hog.setSVMDetector(cv::HOGDescriptor::getDefaultPeopleDetector());

    while (true) {
      // Receive TLV-encoded data and deserialize it into JSON
      auto encodedJson = receive();

      measurementHandler->setField(std::to_string(computeCounter), "CMP_START",
                                   0);
      measurementHandler->setField(std::to_string(computeCounter), "IS->PC", 0);

      nlohmann::json deserializedData = Serde::deserialize(encodedJson);
      if (deserializedData["image"].empty()) {
        std::cerr << "Received empty Image, cannot decode." << std::endl;
        continue;
      }

      else {
        // Extract frameID and encodedImage from the deserialized JSON
        int frameID = deserializedData["frameID"];
        std::vector<uint8_t> encodedImage =
            deserializedData["image"].get_binary();

        // Decode the image (JPEG format) using OpenCV
        cv::Mat greyImage = cv::imdecode(encodedImage, cv::IMREAD_COLOR);

        NDN_LOG_INFO("Decoded: " << greyImage.total() * greyImage.elemSize()
                                 << " bytes");

        int numPeople = detect(hog, greyImage);

        nlohmann::json resultData;
        resultData["frameID"] = frameID;
        resultData["PeopleCount"] = std::to_string(numPeople);
        NDN_LOG_INFO("People count: " << numPeople);

        std::vector<uint8_t> encodedAnalytics = Serde::serialize(resultData);
        std::cout << "People Count:\n"
                  << "FrameID:" << frameID << "\n"
                  << "People Found:" << numPeople << "\n"
                  << "Encoded result Size: " << encodedAnalytics.size()
                  << "bytes" << std::endl;
        std::cout << "------------------------------------" << std::endl;
        // Push the cropped face for further processing
        push(encodedAnalytics);
      }

      // ##### MEASUREMENT #####
      measurementHandler->setField(std::to_string(computeCounter), "PC->AGG",
                                   0);
      measurementHandler->setField(std::to_string(computeCounter), "CMP_FINISH",
                                   0);
      computeCounter++;
    }
  }

  int detect(const cv::HOGDescriptor &hog, cv::Mat &img) {
    std::vector<cv::Rect> found, foundFiltered;
    // Perform HOG detection with optimized parameters
    hog.detectMultiScale(img, found, 0, cv::Size(8, 8), cv::Size(32, 32), 1.05,
                         2);

    // Filter overlapping boxes
    for (size_t i = 0; i < found.size(); i++) {
      cv::Rect r = found[i];
      size_t j;
      for (j = 0; j < found.size(); j++) {
        if (j != i && (r & found[j]) == r) {
          break;
        }
      }
      if (j == found.size()) {
        foundFiltered.push_back(r);
      }
    }

    // Draw detections
    for (size_t i = 0; i < foundFiltered.size(); i++) {
      cv::Rect r = foundFiltered[i];
      cv::rectangle(img, r.tl(), r.br(), cv::Scalar(0, 255, 0), 3);
    }

    return foundFiltered.size();
  }

private:
  std::vector<std::vector<int>> bboxes;
};

void run(const std::string &syncPrefix, const std::string &nodePrefix,
         const std::string &subTopic, const std::string &pubTopic,
         uint32_t numberOfProducerPartitions,
         std::vector<uint32_t> consumerPartitions,
         std::chrono::milliseconds publishInterval) {

  PeopleCounter peopleCount;
  ndn::Face face;

  auto iceflow =
      std::make_shared<iceflow::IceFlow>(syncPrefix, nodePrefix, face);

  auto producer = iceflow::IceflowProducer(
      iceflow, pubTopic, numberOfProducerPartitions, publishInterval);

  auto consumer =
      iceflow::IceflowConsumer(iceflow, subTopic, consumerPartitions);

  std::vector<std::thread> threads;
  threads.emplace_back(&iceflow::IceFlow::run, iceflow);
  threads.emplace_back([&peopleCount, &consumer, &producer]() {
    peopleCount.peopleCount(
        [&consumer]() {
          std::vector<uint8_t> encodedImage = consumer.receiveData();

          return encodedImage;
        },
        [&producer](const std::vector<uint8_t> &analytics) {
          producer.pushData(analytics);
        });
  });

  for (auto &thread : threads) {
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
        consumerPartitions, std::chrono::milliseconds(publishInterval));
  }

  catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}
