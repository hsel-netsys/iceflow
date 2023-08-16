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

#include <csignal>
#include <thread>

#include "yaml-cpp/yaml.h"

#include "iceflow/block.hpp"
#include "iceflow/measurements.hpp"
#include "iceflow/producer-tlv.hpp"

iceflow::Measurement *msCmp;

void signalCallbackHandler(int signum) {
  msCmp->recordToFile();
  // Terminate program
  exit(signum);
}

class ImageSource {

public:
  void compute(const std::string &videoFilename,
               iceflow::RingBuffer<iceflow::Block> *outputQueue,
               int outputThreshold, int frameRate) {
    cv::Mat frame;
    cv::Mat grayFrame;
    cv::VideoCapture cap;
    int frameCounter = 0;
    int computeCounter = 0;

    cap.open(videoFilename);
    NDN_LOG_INFO("Video Frame Rate: " << cap.get(cv::CAP_PROP_FPS));
    NDN_LOG_INFO("Number of Frames of the input video: "
                 << cap.get(cv::CAP_PROP_FRAME_COUNT));
    NDN_LOG_INFO("Frame Processing Rate : " << frameRate);
    while (cv::waitKey(1) < 0) {
      auto start = std::chrono::system_clock::now();
      msCmp->setField(std::to_string(computeCounter), "CMP_START", 0);
      cap.set(cv::CAP_PROP_POS_FRAMES, frameCounter);
      cap.read(frame);
      frameCounter += frameRate;
      if (frame.empty()) {
        cv::waitKey();
        break;
      }
      // create grey frame from the captured frame
      cvtColor(frame, grayFrame, cv::COLOR_BGR2GRAY);

      iceflow::Block resultBlock;
      nlohmann::json imageJson = toJsonEncoded(grayFrame);
      m_jsonOutput.setJson(imageJson);

      std::pair<iceflow::JsonData, cv::Mat> result;
      result.first = m_jsonOutput;
      result.second = grayFrame;

      resultBlock.pushJson(m_jsonOutput);
      resultBlock.pushFrameCompress(grayFrame);
      msCmp->setField(std::to_string(computeCounter), "CMP_FINISH", 0);
      // pass the frame and metaInfo to the producer Queue

      NDN_LOG_INFO(std::to_string(imageJson["frameID"].get<int>()));

      auto end = std::chrono::system_clock::now();
      std::chrono::duration<double> elapsedTime = (end - start);
      NDN_LOG_INFO("Image Source Compute Time: " << elapsedTime.count());

      cv::waitKey(1000 / frameRate);
      auto blockingTimeStart = std::chrono::system_clock::now();
      outputQueue->pushData(resultBlock, outputThreshold);
      NDN_LOG_INFO("Output Queue Size: " << outputQueue->size());
      auto blockingTimeEnd = std::chrono::system_clock::now();

      std::chrono::duration<double> elapsedBlockingTime =
          (blockingTimeEnd - blockingTimeStart);

      NDN_LOG_INFO("Blocking Time: " << elapsedBlockingTime.count());
      NDN_LOG_INFO("Absolute Compute time for Frame "
                   << imageJson["frameID"] << ":"
                   << (elapsedTime - elapsedBlockingTime).count());

      // ##### MEASUREMENT #####
      msCmp->setField(std::to_string(imageJson["frameID"].get<int>()), "IS->FD",
                      0);
      msCmp->setField(std::to_string(imageJson["frameID"].get<int>()), "IS->PC",
                      0);
      msCmp->setField(std::to_string(imageJson["frameID"].get<int>()),
                      "IS->PC2", 0);
      msCmp->setField(std::to_string(imageJson["frameID"].get<int>()),
                      "IS->PC3", 0);
      msCmp->setField(std::to_string(imageJson["frameID"].get<int>()),
                      "IS->PC4", 0);
      msCmp->setField(std::to_string(imageJson["frameID"].get<int>()),
                      "IS->AGG", 0);

      computeCounter++;
    }
  }

  nlohmann::json toJsonEncoded(cv::Mat image) {
    m_frameId++;
    nlohmann::json j = {};
    j["frameID"] =
        m_frameId; // pass the same id when processing this//to measure latency
    j["width"] = image.cols;
    j["height"] = image.rows;
    j["depth"] = image.depth();
    j["channels"] = image.channels();
    j["type"] = "data";
    return j;
  }

private:
  int m_frameId = 0;
  nlohmann::json m_json1 = {};
  iceflow::JsonData m_jsonOutput;
};

void DataFlow(std::string &pubSyncPrefix, const std::string &userPrefixDataMain,
              const std::string &userPrefixDataManifest,
              const std::string &userPrefixAck, int nDataStreams,
              int publishInterval, int publishIntervalNew, int namesInManifest,
              std::string fileName, int outputThreshold, int frameRate,
              int mapThreshold) {

  // Data producer
  auto *simpleProducer = new iceflow::ProducerTlv(
      pubSyncPrefix, userPrefixDataMain, userPrefixDataManifest, userPrefixAck,
      nDataStreams, publishInterval, publishIntervalNew, mapThreshold);

  auto *compute = new ImageSource();

  std::thread th1(&ImageSource::compute, compute, fileName,
                  &simpleProducer->outputQueueBlock, outputThreshold,
                  frameRate);

  // Data
  std::thread th2(&iceflow::ProducerTlv::runPro, simpleProducer);

  std::vector<std::thread> ProducerThreads;
  ProducerThreads.push_back(std::move(th1));
  ProducerThreads.push_back(std::move(th2));
  usleep(200000);

  for (auto &t : ProducerThreads) {
    t.join();
  }
}

int main(int argc, char *argv[]) {
  if (argc != 4) {
    std::cout << "usage: " << argv[0] << " <config-file><input-file><test-name>"
              << std::endl;
    return 1;
  }

  YAML::Node config = YAML::LoadFile(argv[1]);

  auto pubSyncPrefix = config["Producer"]["pubSyncPrefix"].as<std::string>();
  auto userPrefixDataMain =
      config["Producer"]["userPrefixDataMain"].as<std::string>();
  auto userPrefixDataManifest =
      config["Producer"]["userPrefixDataManifest"].as<std::string>();
  auto userPrefixAck = config["Producer"]["userPrefixAck"].as<std::string>();
  int nDataStreams = config["Producer"]["nDataStreams"].as<int>();
  int publishInterval = config["Producer"]["publishInterval"].as<int>();
  int publishIntervalNew = config["Producer"]["publishIntervalNew"].as<int>();
  int outputThreshold = config["Producer"]["outputThreshold"].as<int>();
  int namesInManifest = config["Producer"]["namesInManifest"].as<int>();
  std::string fileName = argv[2];
  int frameRate = config["Producer"]["frameRate"].as<int>();
  int mapThreshold = config["Producer"]["mapThreshold"].as<int>();

  // ##### MEASUREMENT #####
  std::string measurementName = argv[3];
  std::string nodeName = config["Measurement"]["nodeName"].as<std::string>();
  int saveInterval = config["Measurement"]["saveInterval"].as<int>();

  ::signal(SIGINT, signalCallbackHandler);
  msCmp =
      new iceflow::Measurement(measurementName, nodeName, saveInterval, "A");

  try {
    DataFlow(pubSyncPrefix, userPrefixDataMain, userPrefixDataManifest,
             userPrefixAck, nDataStreams, publishInterval, publishIntervalNew,
             namesInManifest, fileName, outputThreshold, frameRate,
             mapThreshold);

  } catch (const std::exception &e) {
    std::cout << (e.what()) << std::endl;
  }
}
