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
#include "iceflow/dag-parser.hpp"
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
  void imageSource(const std::string &videoFilename,
                   std::function<void(std::vector<uint8_t>)> push) {

    cv::Mat frame;
    cv::Mat grayFrame;
    cv::VideoCapture cap;

    cap.open(videoFilename);

    // Check if the video was opened successfully
    if (!cap.isOpened()) {
      std::cerr << "Error: Could not open video file " << videoFilename
                << std::endl;
      return;
    }
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
                << "FrameID:" << frameID << "\n"
                << "Encoded Image Size: " << serializedData.size() << " bytes"
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

void run(const std::string &nodeName, const std::string &dagFileName,
         const std::string &videoFile) {
  std::cout << "Starting IceFlow Stream Processing - - - -" << std::endl;
  ImageSource inputImage;
  ndn::Face face;

  auto dagParser = iceflow::DAGParser::parseFromFile(dagFileName);

  auto iceflow = std::make_shared<iceflow::IceFlow>(dagParser, nodeName, face);

  auto node = dagParser.findNodeByName(nodeName);

  if (!node.downstream.empty()) {
    auto downstreamEdgeName = node.downstream.at(0).id;
    std::cout << "Downstream edge: " << downstreamEdgeName << std::endl;
    auto applicationConfiguration = node.applicationConfiguration;

    auto saveThreshold =
        applicationConfiguration.at("measurementsSaveThreshold")
            .get<uint64_t>();

    ::signal(SIGINT, signalCallbackHandler);
    measurementHandler = new iceflow::Measurement(
        nodeName, iceflow->getNodePrefix(), saveThreshold, "A");
    std::vector<std::thread> threads;
    threads.emplace_back(&iceflow::IceFlow::run, iceflow);
    threads.emplace_back([&inputImage, &iceflow, videoFile,
                          &downstreamEdgeName]() {
      inputImage.imageSource(videoFile, [&iceflow, &downstreamEdgeName](
                                            const std::vector<uint8_t> &data) {
        iceflow->pushData(downstreamEdgeName, data);
      });
    });
    for (auto &thread : threads) {
      thread.join();
    }
  } else {
    std::cerr << "Error: No downstream nodes found in the DAG." << std::endl;
    return;
  }
}

int main(int argc, const char *argv[]) {

  if (argc != 3) {
    std::string command = argv[0];
    std::cout << "usage: " << command << " <application-dag-file><video-file>"
              << std::endl;
    return 1;
  }

  try {
    std::string nodeName = "imageSource";
    std::string dagFileName = argv[1];
    std::string videoFile = argv[2];

    run(nodeName, dagFileName, videoFile);

  } catch (const std::exception &e) {
    std::cout << e.what() << std::endl;
  }

  return 0;
}
