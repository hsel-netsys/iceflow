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

#include "iceflow/consumer.hpp"
#include "iceflow/iceflow.hpp"
#include "iceflow/measurements.hpp"
#include "iceflow/producer.hpp"

#include <csignal>
#include <iostream>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/logger.hpp>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <yaml-cpp/yaml.h>

#include <opencv2/dnn.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>

NDN_LOG_INIT(iceflow.examples.visionprocessing.faceDetection);

iceflow::Measurement *measurementHandler;

void signalCallbackHandler(int signum) {
  measurementHandler->recordToFile();
  exit(signum);
}

class FaceDetection {
public:
  void faceDetection(std::function<cv::Mat()> receive,
                     std::function<void(cv::Mat)> push,
                     std::string protobufBinaryFileName,
                     std::string mlModelFileName) {

    cv::Scalar MODEL_MEAN_VALUES =
        cv::Scalar(78.4263377603, 87.7689143744, 114.895847746);
    cv::dnn::Net faceNet =
        cv::dnn::readNet(mlModelFileName, protobufBinaryFileName);
    cv::Mat frameFace;
    int padding = 20;
    int computeCounter = 0;

    while (true) {
      auto greyFrame = receive();

      measurementHandler->setField(std::to_string(computeCounter), "CMP_START",
                                   0);

      NDN_LOG_INFO("Consumed grey frame: "
                   << computeCounter << greyFrame.total() * greyFrame.elemSize()
                   << " Bytes");

      if (!greyFrame.empty()) {
        measurementHandler->setField(std::to_string(computeCounter), "IS->FD",
                                     0);

        // Get the bounding boxes of detected faces
        tie(frameFace, bboxes) = getFaceBox(faceNet, greyFrame, 0.7);

        for (auto it = begin(bboxes); it != end(bboxes); it++) {
          // Get the bounding box coordinates
          int x1 = std::max(0, it->at(0) - padding);
          int y1 = std::max(0, it->at(1) - padding);
          int x2 = std::min(greyFrame.cols, it->at(2) + padding);
          int y2 = std::min(greyFrame.rows, it->at(3) + padding);

          // Create a Rect representing the cropped face region
          cv::Rect faceRect(x1, y1, x2 - x1, y2 - y1);

          // Ensure the rectangle is valid
          if (faceRect.width > 0 && faceRect.height > 0) {
            // Crop the face from the grey frame
            cv::Mat greyface = greyFrame(faceRect);

            // Log the size of the detected face
            std::cout << "Detected face: "
                      << greyface.total() * greyface.elemSize() << " bytes"
                      << std::endl;

            // Push the cropped face for further processing
            push(greyface);
          }
        }
      }

      // ##### MEASUREMENT #####
      measurementHandler->setField(std::to_string(computeCounter), "FD->AD", 0);
      measurementHandler->setField(std::to_string(computeCounter), "FD->GD", 0);
      measurementHandler->setField(std::to_string(computeCounter), "FD->ED", 0);
      measurementHandler->setField(std::to_string(computeCounter), "CMP_FINISH",
                                   0);
      computeCounter++;
    }
  }

  static std::tuple<cv::Mat, std::vector<std::vector<int>>>
  getFaceBox(cv::dnn::Net net, cv::Mat &frame, double confidenceThreshold) {
    cv::Mat frameOpenCVDNN = frame.clone();
    int frameHeight = frameOpenCVDNN.rows;
    int frameWidth = frameOpenCVDNN.cols;
    double inScaleFactor = 1.0;
    cv::Size size = cv::Size(300, 300);
    cv::Scalar meanVal = cv::Scalar(104, 117, 123);

    cv::Mat inputBlob;
    inputBlob = cv::dnn::blobFromImage(frameOpenCVDNN, inScaleFactor, size,
                                       meanVal, true, false);

    net.setInput(inputBlob, "data");
    cv::Mat detection = net.forward("detection_out");

    cv::Mat detectionMat(detection.size[2], detection.size[3], CV_32F,
                         detection.ptr<float>());

    std::vector<std::vector<int>> bboxes;

    for (int i = 0; i < detectionMat.rows; i++) {
      float confidence = detectionMat.at<float>(i, 2);

      if (confidence > confidenceThreshold) {
        int x1 = static_cast<int>(detectionMat.at<float>(i, 3) * frameWidth);
        int y1 = static_cast<int>(detectionMat.at<float>(i, 4) * frameHeight);
        int x2 = static_cast<int>(detectionMat.at<float>(i, 5) * frameWidth);
        int y2 = static_cast<int>(detectionMat.at<float>(i, 6) * frameHeight);
        std::vector<int> box = {x1, y1, x2, y2};
        bboxes.push_back(box);
        cv::rectangle(frameOpenCVDNN, cv::Point(x1, y1), cv::Point(x2, y2),
                      cv::Scalar(0, 255, 0), 2, 4);
      }
    }

    return std::make_tuple(frameOpenCVDNN, bboxes);
  }

private:
  std::vector<std::vector<int>> bboxes;
};

void run(const std::string &syncPrefix, const std::string &nodePrefix,
         const std::string &subTopic, const std::string &pubTopic,
         uint32_t numberOfProducerPartitions,
         std::vector<uint32_t> consumerPartitions,
         std::chrono::milliseconds publishInterval,
         const std::string &protobufBinaryFileName,
         const std::string &mlModelFileName) {

  FaceDetection facialDetection;
  ndn::Face face;

  auto iceflow =
      std::make_shared<iceflow::IceFlow>(syncPrefix, nodePrefix, face);

  auto producer = iceflow::IceflowProducer(
      iceflow, pubTopic, numberOfProducerPartitions, publishInterval);

  auto consumer =
      iceflow::IceflowConsumer(iceflow, subTopic, consumerPartitions);

  std::vector<std::thread> threads;
  threads.emplace_back(&iceflow::IceFlow::run, iceflow);
  threads.emplace_back([&facialDetection, &consumer, &producer,
                        &protobufBinaryFileName, &mlModelFileName]() {
    facialDetection.faceDetection(
        [&consumer]() -> cv::Mat {
          auto encodedImage = consumer.receiveData();

          // Check if the received image data is empty
          if (encodedImage.empty()) {
            std::cerr << "Received empty data, cannot decode." << std::endl;
            return cv::Mat();
          }

          NDN_LOG_INFO("Received: " << encodedImage.size() << " bytes");

          cv::Mat greyImage = cv::imdecode(encodedImage, cv::IMREAD_COLOR);

          if (greyImage.empty()) {
            std::cerr << "Error: Could not decode received data into image."
                      << std::endl;
            return cv::Mat();
          }
          NDN_LOG_INFO("Decoded: " << greyImage.total() * greyImage.elemSize()
                                   << " bytes");

          return greyImage;
        },
        [&producer](const cv::Mat &greyImage) {
          std::vector<uint8_t> encodedCroppedFace;
          auto encodingSuccess =
              cv::imencode(".jpg", greyImage, encodedCroppedFace);
          if (!encodingSuccess) {
            std::cerr << "Error: Could not encode image." << std::endl;
            return;
          }
          producer.pushData(encodedCroppedFace);
        },
        protobufBinaryFileName, mlModelFileName);
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
  std::string protobufBinaryFileName = argv[3];
  std::string mlModelFileName = argv[4];

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
        protobufBinaryFileName, mlModelFileName);
  }

  catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}
