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
#include "iceflow/dag-parser.hpp"
#include "iceflow/iceflow.hpp"
#include "iceflow/measurements.hpp"
#include "iceflow/producer.hpp"
#include "iceflow/serde.hpp"

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/logger.hpp>

#include <csignal>
#include <functional>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <vector>

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

class FaceDetector {
public:
  FaceDetector(const std::string &protobufFile, const std::string &mlModel)
      : m_faceNet(cv::dnn::readNet(mlModel, protobufFile)) {}

  void detectFaces(std::vector<uint8_t> encodedCropped,
                   std::function<void(std::vector<uint8_t>)> push) {

    nlohmann::json deserializedData = Serde::deserialize(encodedCropped);

    if (deserializedData["image"].empty()) {
      std::cerr << "Received empty Image, cannot decode." << std::endl;
      return;
    } else {
      int frameID = deserializedData["frameID"];
      std::vector<uint8_t> encodedImage =
          deserializedData["image"].get_binary();

      cv::Mat greyImage = cv::imdecode(encodedImage, cv::IMREAD_COLOR);
      measurementHandler->setField(std::to_string(m_computeCounter),
                                   "CMP_START", 0);
      measurementHandler->setField(std::to_string(frameID), "IS->FD", 0);

      std::tie(m_frameFace, m_bboxes) = getFaceBox(m_faceNet, greyImage, 0.7);
      auto greyFace = cropFaceRoi(greyImage);

      std::vector<uint8_t> encodedCroppedFace;
      if (!cv::imencode(".jpeg", greyFace, encodedCroppedFace)) {
        std::cerr << "Error: Could not encode image." << std::endl;
        return;
      }

      auto encodedFaceResult = serializeResults(frameID, encodedCroppedFace);

      push(encodedFaceResult);

      measurementHandler->setField(std::to_string(frameID), "FD->AD", 0);
      measurementHandler->setField(std::to_string(frameID), "FD->GD", 0);
      measurementHandler->setField(std::to_string(m_computeCounter),
                                   "CMP_FINISH", 0);
      m_computeCounter++;
    }
  }

  cv::Mat cropFaceRoi(const cv::Mat &greyImage) {
    for (const auto &box : m_bboxes) {
      int x1 = std::max(0, box[0] - padding);
      int y1 = std::max(0, box[1] - padding);
      int x2 = std::min(greyImage.cols, box[2] + padding);
      int y2 = std::min(greyImage.rows, box[3] + padding);

      cv::Rect faceRect(x1, y1, x2 - x1, y2 - y1);

      if (faceRect.width > 0 && faceRect.height > 0) {
        cv::Mat greyFace = greyImage(faceRect);

        NDN_LOG_INFO("Detected face: " << greyFace.total() * greyFace.elemSize()
                                       << " bytes \n");

        return greyFace;
      }
    }
    return cv::Mat();
  }

  static std::tuple<cv::Mat, std::vector<std::vector<int>>>
  getFaceBox(cv::dnn::Net net, cv::Mat &frame, double confidenceThreshold) {
    cv::Mat frameOpenCVDNN = frame.clone();
    int frameHeight = frameOpenCVDNN.rows;
    int frameWidth = frameOpenCVDNN.cols;
    double inScaleFactor = 1.0;
    cv::Size size = cv::Size(300, 300);

    cv::Scalar MODEL_MEAN_VALUES =
        cv::Scalar(78.4263377603, 87.7689143744, 114.895847746);

    cv::Mat inputBlob = cv::dnn::blobFromImage(
        frameOpenCVDNN, inScaleFactor, size, MODEL_MEAN_VALUES, true, false);

    net.setInput(inputBlob, "data");
    cv::Mat detection = net.forward("detection_out");

    cv::Mat detectionMat(detection.size[2], detection.size[3], CV_32F,
                         detection.ptr<float>());

    std::vector<std::vector<int>> rectangles;

    for (int i = 0; i < detectionMat.rows; i++) {
      float confidence = detectionMat.at<float>(i, 2);

      if (confidence > confidenceThreshold) {
        int x1 = static_cast<int>(detectionMat.at<float>(i, 3) * frameWidth);
        int y1 = static_cast<int>(detectionMat.at<float>(i, 4) * frameHeight);
        int x2 = static_cast<int>(detectionMat.at<float>(i, 5) * frameWidth);
        int y2 = static_cast<int>(detectionMat.at<float>(i, 6) * frameHeight);
        std::vector<int> box = {x1, y1, x2, y2};
        rectangles.push_back(box);
        cv::rectangle(frameOpenCVDNN, cv::Point(x1, y1), cv::Point(x2, y2),
                      cv::Scalar(0, 255, 0), 2, 4);
      }
    }

    return std::make_tuple(frameOpenCVDNN, rectangles);
  }

  std::vector<uint8_t> serializeResults(int frameCounter,
                                        std::vector<uint8_t> faceEncoded) {
    nlohmann::json resultData;
    resultData["frameID"] = frameCounter;
    resultData["image"] = nlohmann::json::binary(faceEncoded);

    std::vector<uint8_t> serializedResults = Serde::serialize(resultData);

    NDN_LOG_INFO("FaceDetection: \n"
                 << " FrameID:" << frameCounter << "\n"
                 << " Encoded Image Size: " << serializedResults.size()
                 << " bytes"
                 << "\n"
                 << "------------------------------------");
    return serializedResults;
  }

private:
  std::vector<std::vector<int>> m_bboxes;
  cv::Mat m_frameFace;
  int padding = 20;
  int m_computeCounter = 0;
  cv::dnn::Net m_faceNet;
};

void run(const std::string &nodeName, const std::string &dagFileName,
         const std::string &protobufFile, const std::string &mlModel) {

  FaceDetector faceDetector(protobufFile, mlModel);
  ndn::Face face;

  auto dagParser = iceflow::DAGParser::parseFromFile(dagFileName);
  auto node = dagParser.findNodeByName(nodeName);

  auto upstreamEdge = dagParser.findUpstreamEdges(node).at(0).second;
  auto upstreamEdgeName = upstreamEdge.id;

  auto applicationConfiguration = node.applicationConfiguration;
  auto saveThreshold =
      applicationConfiguration.at("measurementsSaveThreshold").get<uint64_t>();

  auto iceflow = std::make_shared<iceflow::IceFlow>(dagParser, nodeName, face);

  ::signal(SIGINT, signalCallbackHandler);
  measurementHandler = new iceflow::Measurement(
      nodeName, iceflow->getNodePrefix(), saveThreshold, "A");

  auto prosumerCallback = [&iceflow, &faceDetector, &node](
                              const std::vector<uint8_t> &encodedCroppedFace,
                              iceflow::ProducerCallback producerCallback) {
    for (auto downstream : node.downstream) {
      auto pushDataCallback = [downstream, producerCallback](
                                  std::vector<uint8_t> encodedCroppedFace) {
        auto downstreamEdgeName = downstream.id;
        producerCallback(downstreamEdgeName, encodedCroppedFace);
      };
      faceDetector.detectFaces(encodedCroppedFace, pushDataCallback);
    }
  };

  iceflow->registerProsumerCallback(upstreamEdgeName, prosumerCallback);

  iceflow->repartitionConsumer(upstreamEdgeName, {0});
  iceflow->run();
}

int main(int argc, const char *argv[]) {
  if (argc != 4) {
    std::cout << "usage: " << argv[0]
              << " <application-dag-file><protobuf_binary> "
                 "<ML-Model>"
              << std::endl;
    return 1;
  }

  try {
    std::string nodeName = "faceDetection";
    std::string dagFileName = argv[1];
    std::string protobufFile = argv[2];
    std::string mlModel = argv[3];

    run(nodeName, dagFileName, protobufFile, mlModel);
  } catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}
