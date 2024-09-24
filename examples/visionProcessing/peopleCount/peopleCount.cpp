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
  PeopleCounter() {
    m_hog.setSVMDetector(cv::HOGDescriptor::getDefaultPeopleDetector());
  }
  void Countpeople(std::vector<uint8_t> encodedCropped,
                   std::function<void(std::vector<uint8_t>)> push) {

    measurementHandler->setField(std::to_string(m_computeCounter), "CMP_START",
                                 0);
    measurementHandler->setField(std::to_string(m_computeCounter), "IS->PC", 0);

    nlohmann::json deserializedData = Serde::deserialize(encodedCropped);
    if (deserializedData["image"].empty()) {
      std::cerr << "Received empty Image, cannot decode." << std::endl;
      return;
    }

    else {

      int frameID = deserializedData["frameID"];
      std::vector<uint8_t> encodedImage =
          deserializedData["image"].get_binary();

      cv::Mat greyImage = cv::imdecode(encodedImage, cv::IMREAD_COLOR);

      NDN_LOG_INFO("Decoded: " << greyImage.total() * greyImage.elemSize()
                               << " bytes");

      int numPeople = detect(m_hog, greyImage);
      auto encodedAnalytics = serializeResults(frameID, numPeople);

      push(encodedAnalytics);

      measurementHandler->setField(std::to_string(frameID), "PC->AGG", 0);
      measurementHandler->setField(std::to_string(m_computeCounter),
                                   "CMP_FINISH", 0);
    }

    m_computeCounter++;
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

  std::vector<uint8_t> serializeResults(int &frameCounter, int &numPeople) {
    nlohmann::json resultData;
    resultData["frameID"] = frameCounter;
    resultData["PeopleCount"] = std::to_string(numPeople);

    std::vector<uint8_t> serializedResults = Serde::serialize(resultData);
    NDN_LOG_INFO("People Count:\n"
                 << "FrameID:" << frameCounter << "\n"
                 << "People Found:" << numPeople << "\n"
                 << "Encoded result Size: " << serializedResults.size()
                 << "bytes"
                 << "\n"
                 << "------------------------------------");

    return serializedResults;
  }

private:
  std::vector<std::vector<int>> m_bboxes;
  cv::HOGDescriptor m_hog;
  int m_computeCounter = 0;
};

void run(const std::string &nodeName, const std::string &dagFileName) {

  PeopleCounter peopleCounter;
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
  auto prosumerCallback = [&iceflow, &peopleCounter, &node](
                              const std::vector<uint8_t> &encodedCroppedFace,
                              iceflow::ProducerCallback producerCallback) {
    for (auto downstream : node.downstream) {
      auto pushDataCallback = [downstream, producerCallback](
                                  std::vector<uint8_t> encodedAnalytics) {
        auto downstreamEdgeName = downstream.id;
        producerCallback(downstreamEdgeName, encodedAnalytics);
      };
      peopleCounter.Countpeople(encodedCroppedFace, pushDataCallback);
    }
  };

  iceflow->registerProsumerCallback(upstreamEdgeName, prosumerCallback);
  iceflow->repartitionConsumer(upstreamEdgeName, {0});
  iceflow->run();
}

int main(int argc, const char *argv[]) {
  if (argc != 2) {
    std::cout << "usage: " << argv[0] << " <application-dag-file>" << std::endl;
    return 1;
  }

  try {
    std::string nodeName = "peopleCounter";
    std::string dagFileName = argv[1];

    run(nodeName, dagFileName);
  } catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}
