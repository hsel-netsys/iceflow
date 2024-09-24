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

#include <ndn-cxx/util/logger.hpp>
#include <opencv2/opencv.hpp>

NDN_LOG_INIT(iceflow.examples.visionprocessing.imageSource);

iceflow::Measurement *measurementHandler;

void signalCallbackHandler(int signum) {
  measurementHandler->recordToFile();
  exit(signum);
}

class ImageFeeder {
public:
  void SourceImage(
      const std::string &videoFilename,
      std::function<void(std::vector<uint8_t>, const std::string &)> push) {

    cv::Mat frame;
    cv::Mat grayFrame;
    cv::VideoCapture cap;
    int computeCounter = 0;

    cap.open(videoFilename);
    if (!cap.isOpened()) {
      std::cerr << "Error: Could not open video file " << videoFilename
                << std::endl;
      return;
    }

    while (cv::waitKey(1) < 0) {

      auto start = std::chrono::system_clock::now();
      measurementHandler->setField(std::to_string(computeCounter), "CMP_START",
                                   0);

      cap.read(frame);
      if (frame.empty()) {
        break;
      }

      cv::cvtColor(frame, grayFrame, cv::COLOR_BGR2GRAY);

      std::vector<uint8_t> encodedVideoFrame;
      if (!cv::imencode(".jpeg", grayFrame, encodedVideoFrame)) {
        std::cerr << "Error: Could not encode image to JPEG format."
                  << std::endl;
        return;
      }

      auto serializedData = serializeResults(m_frameID, encodedVideoFrame);

      push(serializedData, "is2fd");
      push(serializedData, "is2pc");

      measurementHandler->setField(std::to_string(computeCounter), "IS->FD", 0);
      measurementHandler->setField(std::to_string(computeCounter), "IS->PC", 0);
      measurementHandler->setField(std::to_string(computeCounter), "CMP_FINISH",
                                   0);

      computeCounter++;
      m_frameID++;
    }
  }

  std::vector<uint8_t>
  serializeResults(const int &frameCounter,
                   const std::vector<uint8_t> &encodedVideoFrame) {
    nlohmann::json resultData;
    resultData["frameID"] = frameCounter;
    resultData["image"] = nlohmann::json::binary(encodedVideoFrame);

    auto serializedResults = Serde::serialize(resultData);

    NDN_LOG_INFO("ImageSource:\n"
                 << "FrameID:" << m_frameID << "\n"
                 << "Encoded Image Size: " << serializedResults.size()
                 << " bytes"
                 << "\n"
                 << "------------------------------------");

    return serializedResults;
  }

private:
  int m_frameID = 1;
};

void run(const std::string &nodeName, const std::string &dagFileName,
         const std::string &videoFile) {
  std::cout << "Starting IceFlow Stream Processing - - - -" << std::endl;
  ImageFeeder inputImage;
  ndn::Face face;

  auto dagParser = iceflow::DAGParser::parseFromFile(dagFileName);
  auto iceflow = std::make_shared<iceflow::IceFlow>(dagParser, nodeName, face);
  auto node = dagParser.findNodeByName(nodeName);

  auto applicationConfiguration = node.applicationConfiguration;
  auto saveThreshold =
      applicationConfiguration.at("measurementsSaveThreshold").get<uint64_t>();

  ::signal(SIGINT, signalCallbackHandler);
  measurementHandler = new iceflow::Measurement(
      nodeName, iceflow->getNodePrefix(), saveThreshold, "A");

  std::vector<std::thread> threads;
  threads.emplace_back(&iceflow::IceFlow::run, iceflow);
  threads.emplace_back([&inputImage, &iceflow, videoFile]() {
    inputImage.SourceImage(videoFile,
                           [&iceflow](const std::vector<uint8_t> &data,
                                      const std::string &edgeName) {
                             iceflow->pushData(edgeName, data);
                           });
  });

  for (auto &thread : threads) {
    thread.join();
  }
}

int main(int argc, const char *argv[]) {
  if (argc != 3) {
    std::string command = argv[0];
    std::cout << "usage: " << command << " <application-dag-file> <video-file>"
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
