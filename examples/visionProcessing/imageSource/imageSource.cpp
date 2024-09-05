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

#include "iceflow/iceflow.hpp"
#include "iceflow/measurements.hpp"
#include "iceflow/producer.hpp"

#include <csignal>
#include <fstream>
#include <iostream>
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
                   std::function<void(cv::Mat)> push) {

    auto application = applicationName;
    std::cout << "Starting " << application << " Application - - - - "
              << std::endl;

    cv::Mat frame;
    cv::Mat grayFrame;
    cv::VideoCapture cap;

    cap.open(videoFilename);
    //		cout << "Video Frame Rate: " << cap.get(cv::CAP_PROP_FPS) <<
    // endl;
    //	    cout<<"Number of Frames of the input video: " <<
    // cap.get(cv::CAP_PROP_FRAME_COUNT)<<endl; 		cout << "Frame
    // Processing Rate: "
    //<< frame_rate << endl;

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

      // Calculate and print original frame size in bytes
      //   size_t frameSizeInBytes = frame.total() * frame.elemSize();
      //   std::cout << "Original Frame size: " << frameSizeInBytes << " bytes"
      //             << std::endl;

      // Create grayscale frame from the captured frame
      cv::cvtColor(frame, grayFrame, cv::COLOR_BGR2GRAY);

      // Calculate and print frame size in bytes
      size_t frameSizeInBytes_after = grayFrame.total() * grayFrame.elemSize();

      NDN_LOG_INFO(computeCounter << "th Grey Frame size: "
                                  << frameSizeInBytes_after << " bytes");
      // Save the grayscale frame to the current directory
      // std::string grayFilename = "gray_frame_" +
      // std::to_string(computeCounter) + ".png";
      // cv::imwrite(grayFilename, grayFrame);
      //   std::cout << "Saved grayscale frame as " << grayFilename <<
      //   std::endl;

      push(grayFrame);

      measurementHandler->setField(std::to_string(computeCounter), "IS->FD", 0);
      measurementHandler->setField(std::to_string(computeCounter), "IS->PC", 0);
      measurementHandler->setField(std::to_string(computeCounter), "CMP_FINISH",
                                   0);
      computeCounter++;
    }
  }
};

void run(const std::string &syncPrefix, const std::string &nodePrefix,
         const std::string &pubTopic, uint32_t numberOfPartitions,
         const std::string &videoFile,
         std::chrono::milliseconds publishInterval) {
  std::cout << "Starting IceFlow Stream Processing - - - -" << std::endl;
  ImageSource inputImage;
  ndn::Face face;

  auto iceflow = std::make_shared<iceflow::IceFlow>(syncPrefix, nodePrefix,

                                                    face);
  auto producer = iceflow::IceflowProducer(iceflow, pubTopic,
                                           numberOfPartitions, publishInterval);

  std::vector<std::thread> threads;
  threads.emplace_back(&iceflow::IceFlow::run, iceflow);
  threads.emplace_back([&inputImage, &producer, &nodePrefix, &videoFile]() {
    inputImage.imageSource(
        nodePrefix, videoFile, [&producer](const cv::Mat &data) {
          // Check if data is empty
          if (data.empty()) {
            std::cerr << "Error: Received empty image data." << std::endl;
            return;
          }

          // Encode the grayscale image to a specified format, e.g., JPEG
          std::vector<uint8_t> encodedVideo;
          if (!cv::imencode(".jpg", data, encodedVideo)) {
            std::cerr << "Error: Could not encode image to JPEG format."
                      << std::endl;
            return;
          }

          // Push the encoded data to the producer
          producer.pushData(encodedVideo);
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
