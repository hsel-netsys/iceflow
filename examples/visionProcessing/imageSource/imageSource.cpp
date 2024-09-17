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

#include "iceflow/iceflow.hpp"
#include "iceflow/measurements.hpp"
#include "iceflow/producer.hpp"
#include "iceflow/serde.hpp"

#include <csignal>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>
#include <yaml-cpp/yaml.h>

#include <ndn-cxx/util/logger.hpp>
#include <opencv2/opencv.hpp>

NDN_LOG_INIT(iceflow.examples.visionprocessing.imageSource);

iceflow::Measurement *measurementHandler;

void signalCallbackHandler(int signum) {
  measurementHandler->recordToFile();
  exit(signum);
}

class ImageSource {
public:
  void imageSource(const std::string &applicationName,
                   const std::string &videoFilename,
                   std::function<void(std::vector<uint8_t>)> push) {

    auto application = applicationName;
    std::cout << "Starting " << application << " Application - - - - "
              << std::endl;

    cv::Mat frame;
    cv::Mat grayFrame;
    cv::VideoCapture cap;

    cap.open(videoFilename);

    int computeCounter = 0;

    while (cv::waitKey(1) < 0) {

      auto start = std::chrono::system_clock::now();
      measurementHandler->setField(std::to_string(computeCounter), "CMP_START",
                                   0);

      cap.read(frame);
      if (frame.empty()) {
        cv::waitKey();
        break;
      }

      // Convert the frame to grayscale
      cv::cvtColor(frame, grayFrame, cv::COLOR_BGR2GRAY);

      // Calculate the size of the grayscale image
      size_t frameSizeInBytes_after = grayFrame.total() * grayFrame.elemSize();

      // Check if the grayscale image is valid
      if (grayFrame.empty()) {
        std::cerr << "Error: Received empty image data." << std::endl;
        return;
      }

      // Encode the grayscale image into JPEG format
      std::vector<uint8_t> encodedVideo;
      if (!cv::imencode(".jpeg", grayFrame, encodedVideo)) {
        std::cerr << "Error: Could not encode image to JPEG format."
                  << std::endl;
        return;
      }

      // Create a nlohmann::json object to store frameID and encoded video
      nlohmann::json resultData;
      resultData["frameID"] = frameID;
      resultData["image"] = nlohmann::json::binary(encodedVideo);

      auto serializedData = Serde::serialize(resultData);
      std::cout << "ImageSource:\n"
                << " FrameID:" << frameID << "\n"
                << " Encoded Image Size: " << serializedData.size() << " bytes"
                << std::endl;
      std::cout << "------------------------------------" << std::endl;
      push(serializedData);

      // Log measurements
      measurementHandler->setField(std::to_string(computeCounter), "IS->FD", 0);
      measurementHandler->setField(std::to_string(computeCounter), "IS->PC", 0);
      measurementHandler->setField(std::to_string(computeCounter), "CMP_FINISH",
                                   0);

      computeCounter++;
      frameID++;
    }
  }

private:
  int frameID = 1;
};

void run(const std::string &syncPrefix, const std::string &nodePrefix,
         const std::string &pubTopic, uint32_t numberOfPartitions,
         const std::string &videoFile,
         std::chrono::milliseconds publishInterval) {
  std::cout << "Starting IceFlow Stream Processing - - - -" << std::endl;
  ImageSource inputImage;
  ndn::Face face;

  auto iceflow =
      std::make_shared<iceflow::IceFlow>(syncPrefix, nodePrefix, face);
  auto producer = iceflow::IceflowProducer(iceflow, pubTopic,
                                           numberOfPartitions, publishInterval);

  std::vector<std::thread> threads;
  threads.emplace_back(&iceflow::IceFlow::run, iceflow);
  threads.emplace_back([&inputImage, &producer, &nodePrefix, &videoFile]() {
    inputImage.imageSource(nodePrefix, videoFile,
                           [&producer](const std::vector<uint8_t> &data) {
                             producer.pushData(data);
                           });
  });

  for (auto &thread : threads) {
    thread.join();
  }
}

int main(int argc, const char *argv[]) {

  if (argc != 4) {
    std::string command = argv[0];
    std::cout << "usage: " << command
              << " <config-file> <video-file> <measurement-Name>" << std::endl;
    return 1;
  }

  std::string configFileName = argv[1];
  std::string measurementFileName = argv[3];

  YAML::Node config = YAML::LoadFile(configFileName);
  YAML::Node producerConfig = config["producer"];
  YAML::Node measurementConfig = config["measurements"];

  auto syncPrefix = config["syncPrefix"].as<std::string>();
  auto nodePrefix = config["nodePrefix"].as<std::string>();
  auto pubTopic = producerConfig["topic"].as<std::string>();
  uint64_t publishInterval = producerConfig["publishInterval"].as<uint64_t>();
  auto numberOfPartitions = producerConfig["numberOfPartitions"].as<uint32_t>();
  uint64_t saveThreshold = measurementConfig["saveThreshold"].as<uint64_t>();

  ::signal(SIGINT, signalCallbackHandler);
  measurementHandler = new iceflow::Measurement(measurementFileName, nodePrefix,
                                                saveThreshold, "A");

  try {
    std::string videoStream = argv[2];
    run(syncPrefix, nodePrefix, pubTopic, numberOfPartitions, videoStream,
        std::chrono::milliseconds(publishInterval));
  } catch (const std::exception &e) {
    std::cout << e.what() << std::endl;
  }

  return 0;
}
