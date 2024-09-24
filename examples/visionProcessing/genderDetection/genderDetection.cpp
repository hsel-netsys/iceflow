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
#include <thread>
#include <vector>

#include <opencv2/dnn.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>

NDN_LOG_INIT(iceflow.examples.visionprocessing.genderDetection);

iceflow::Measurement *measurementHandler;

void signalCallbackHandler(int signum) {
  measurementHandler->recordToFile();
  exit(signum);
}

class GenderDetector {
public:
  GenderDetector(const std::string &protobufFile, const std::string &mlModel)
      : m_genderNet(cv::dnn::readNet(mlModel, protobufFile)) {}

  void detectGender(std::vector<uint8_t> encodedCropped,
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
      measurementHandler->setField(std::to_string(frameID), "FD->GD", 0);

      auto genderAnalysis = getGenderPrediction(greyImage);
      auto encodedAnalytics = serializeResults(frameID, genderAnalysis);

      push(encodedAnalytics);

      measurementHandler->setField(std::to_string(frameID), "GD->AGG", 0);
      measurementHandler->setField(std::to_string(m_computeCounter),
                                   "CMP_FINISH", 0);
      m_computeCounter++;
    }
  }

  std::string getGenderPrediction(cv::Mat greyFrame) {
    cv::Mat blob = cv::dnn::blobFromImage(greyFrame, 1, cv::Size(227, 227),
                                          MODEL_MEAN_VALUES, false);

    m_genderNet.setInput(blob);
    cv::Mat genderPreds = m_genderNet.forward();

    std::vector<float> genderPredsVec;
    genderPreds.reshape(1, 1).copyTo(genderPredsVec);

    std::string genderAnalysis = (genderPredsVec[0] > 0.5) ? "Female" : "Male";

    return genderAnalysis;
  }

  std::vector<uint8_t> serializeResults(int &frameCounter,
                                        std::string &analytics) {

    nlohmann::json resultData;
    resultData["frameID"] = frameCounter;
    resultData["Gender"] = analytics;

    std::vector<uint8_t> serializedResults = Serde::serialize(resultData);

    NDN_LOG_INFO("GenderDetection: \n"
                 << " FrameID:" << frameCounter << "\n"
                 << " Gender: " << analytics << "\n"
                 << " Encoded Analytics Size: " << serializedResults.size()
                 << " bytes"
                 << "\n"
                 << "------------------------------------");

    return serializedResults;
  }

private:
  cv::dnn::Net m_genderNet;
  cv::Scalar MODEL_MEAN_VALUES =
      cv::Scalar(78.4263377603, 87.7689143744, 114.895847746);
  int m_computeCounter = 0;
};

void run(const std::string &nodeName, const std::string &dagFileName,
         const std::string &protobufFile, const std::string &mlModel) {

  GenderDetector genderDetector(protobufFile, mlModel);
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
  auto prosumerCallback = [&iceflow, &genderDetector, &node](
                              const std::vector<uint8_t> &encodedCroppedFace,
                              iceflow::ProducerCallback producerCallback) {
    for (auto downstream : node.downstream) {
      auto pushDataCallback = [downstream, producerCallback](
                                  std::vector<uint8_t> encodedAnalytics) {
        auto downstreamEdgeName = downstream.id;
        producerCallback(downstreamEdgeName, encodedAnalytics);
      };
      genderDetector.detectGender(encodedCroppedFace, pushDataCallback);
    }
  };

  iceflow->registerProsumerCallback(upstreamEdgeName, prosumerCallback);
  iceflow->repartitionConsumer(upstreamEdgeName, {0});
  iceflow->run();
}

int main(int argc, const char *argv[]) {
  if (argc != 4) {
    std::cout << "usage: " << argv[0]
              << " <application-dag-file><protobuf_binary><ML-Model>"
              << std::endl;
    return 1;
  }

  try {
    std::string nodeName = "genderDetection";
    std::string dagFileName = argv[1];
    std::string protobufFile = argv[2];
    std::string mlModel = argv[3];

    run(nodeName, dagFileName, protobufFile, mlModel);
  } catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}
