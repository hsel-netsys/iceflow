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

class GenderDetection {
public:
  GenderDetection(const std::string &protobufFile, const std::string &mlModel)
      : genderNet(cv::dnn::readNet(mlModel, protobufFile)) {}

  void genderDetection(std::vector<uint8_t> encodedCropped,
                       std::function<void(std::vector<uint8_t>)> push) {

    nlohmann::json deserializedData = Serde::deserialize(encodedCropped);

    if (deserializedData["image"].empty()) {
      std::cerr << "Received empty Image, cannot decode." << std::endl;
      return;
    } else {
      int frameID = deserializedData["frameID"];
      std::vector<uint8_t> encodedImage =
          deserializedData["image"].get_binary();

      // Decode the image (JPEG format) using OpenCV
      cv::Mat greyImage = cv::imdecode(encodedImage, cv::IMREAD_COLOR);
      measurementHandler->setField(std::to_string(computeCounter), "CMP_START",
                                   0);
      measurementHandler->setField(std::to_string(frameID), "FD->GD", 0);

      cv::Mat blob = cv::dnn::blobFromImage(greyImage, 1, cv::Size(227, 227),
                                            MODEL_MEAN_VALUES, false);

      genderNet.setInput(blob);
      cv::Mat genderPreds = genderNet.forward();

      // Get gender prediction (assumed binary classification, 0 = Male, 1 =
      // Female)
      std::vector<float> genderPredsVec;
      genderPreds.reshape(1, 1).copyTo(genderPredsVec);
      std::string gender = (genderPredsVec[0] > 0.5) ? "Female" : "Male";

      // Serialize results
      nlohmann::json resultData;
      resultData["frameID"] = frameID;
      resultData["Gender"] = gender;

      std::vector<uint8_t> encodedAnalytics = Serde::serialize(resultData);

      std::cout << "GenderDetection: \n"
                << " FrameID:" << frameID << "\n"
                << " Gender: " << gender << "\n"
                << " Encoded Analytics Size: " << encodedAnalytics.size()
                << " bytes" << std::endl;
      std::cout << "------------------------------------" << std::endl;

      push(encodedAnalytics);
      measurementHandler->setField(std::to_string(frameID), "GD->AGG", 0);
      measurementHandler->setField(std::to_string(computeCounter), "CMP_FINISH",
                                   0);
      computeCounter++;
    }
  }

private:
  cv::dnn::Net genderNet;
  cv::Scalar MODEL_MEAN_VALUES =
      cv::Scalar(78.4263377603, 87.7689143744, 114.895847746);
  int computeCounter = 0;
};

std::vector<uint32_t> createConsumerPartitions(uint32_t highestPartitionNumber,
                                               uint32_t consumerIndex,
                                               uint32_t numberOfConsumers) {
  std::vector<uint32_t> consumerPartitions;
  for (auto i = consumerIndex; i <= highestPartitionNumber;
       i += numberOfConsumers) {
    consumerPartitions.push_back(i);
  }
  return consumerPartitions;
}

void run(const std::string &nodeName, const std::string &dagFileName,
         uint32_t consumerIndex, uint32_t numberOfConsumers,
         const std::string &protobufFile, const std::string &mlModel) {

  GenderDetection genderDetection(protobufFile, mlModel);
  ndn::Face face;

  auto dagParser = iceflow::DAGParser::parseFromFile(dagFileName);
  auto node = dagParser.findNodeByName(nodeName);
  if (!node.downstream.empty()) {
    std::string downstreamEdgeName;
    for (const auto &downstream : node.downstream) {
      if (downstream.id == "gd2agg") {
        downstreamEdgeName = downstream.id;
      }
    }

    if (downstreamEdgeName.empty()) {
      std::cerr << "Error: Missing downstream target for gender detection."
                << std::endl;
      return;
    }

    auto upstreamEdge = dagParser.findUpstreamEdges(node).at(0).second;
    auto upstreamEdgeName = upstreamEdge.id;

    auto consumerPartitions = createConsumerPartitions(
        upstreamEdge.maxPartitions, consumerIndex, numberOfConsumers);

    auto applicationConfiguration = node.applicationConfiguration;
    auto saveThreshold =
        applicationConfiguration.at("measurementsSaveThreshold")
            .get<uint64_t>();

    auto iceflow =
        std::make_shared<iceflow::IceFlow>(dagParser, nodeName, face);

    ::signal(SIGINT, signalCallbackHandler);
    measurementHandler = new iceflow::Measurement(
        nodeName, iceflow->getNodePrefix(), saveThreshold, "A");

    auto prosumerCallback = [&iceflow, &genderDetection, &downstreamEdgeName](
                                const std::vector<uint8_t> &encodedCroppedFace,
                                iceflow::ProducerCallback producerCallback) {
      auto pushDataCallback = [downstreamEdgeName, producerCallback](
                                  std::vector<uint8_t> encodedAnalytics) {
        producerCallback(downstreamEdgeName, encodedAnalytics);
      };
      genderDetection.genderDetection(encodedCroppedFace, pushDataCallback);
    };

    iceflow->registerProsumerCallback(upstreamEdgeName, prosumerCallback);
    iceflow->repartitionConsumer(upstreamEdgeName, {0});
    iceflow->run();
  }
}

int main(int argc, const char *argv[]) {
  if (argc != 6) {
    std::cout << "usage: " << argv[0]
              << " <application-dag-file><protobuf_binary> "
                 "<ML-Model><instance-number> <number-of-instances>"
              << std::endl;
    return 1;
  }

  try {
    std::string nodeName = "genderDetection";
    std::string dagFileName = argv[1];
    std::string protobufFile = argv[2];
    std::string mlModel = argv[3];
    int consumerIndex = std::stoi(argv[4]);
    int numberOfConsumers = std::stoi(argv[5]);

    run(nodeName, dagFileName, consumerIndex, numberOfConsumers, protobufFile,
        mlModel);
  } catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}
