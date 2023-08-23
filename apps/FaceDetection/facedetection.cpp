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

#include "iceflow/consumer-tlv.hpp"
#include "iceflow/measurements.hpp"
#include "iceflow/producer-tlv.hpp"

// ###### MEASUREMENT ######
iceflow::Measurement *msCmp;

void signalCallbackHandler(int signum) {
  msCmp->recordToFile();
  // Terminate program
  exit(signum);
}

class FaceDetector {

public:
  [[noreturn]] void compute(iceflow::RingBuffer<iceflow::Block> *input,
                            iceflow::RingBuffer<iceflow::Block> *output,
                            int outputThreshold, std::string ml_proto,
                            std::string ml_model) {

    cv::Scalar MODEL_MEAN_VALUES =
        cv::Scalar(78.4263377603, 87.7689143744, 114.895847746);
    cv::dnn::Net faceNet = cv::dnn::readNet(ml_model, ml_proto);
    cv::Mat frameFace;
    int padding = 20;
    int computeCounter = 0;

    while (true) {

      auto start = std::chrono::system_clock::now();

      NDN_LOG_INFO("Input Queue Size: " << input->size());
      auto inputData = input->waitAndPopValue();
      auto frameData = inputData.pullFrame();
      auto jsonData = inputData.pullJson();
      NDN_LOG_INFO("Compute json: " << jsonData.getJson());
      nlohmann::json inputJson = jsonData.getJson();

      msCmp->setField(std::to_string(computeCounter), "CMP_START", 0);
      if (!frameData.empty()) {
        // ##### MEASUREMENT #####
        msCmp->setField(std::to_string(inputJson["frameID"].get<int>()),
                        "IS->FD", 0);
        tie(frameFace, bboxes) = getFaceBox(faceNet, frameData, 0.7);

        for (auto it = begin(bboxes); it != end(bboxes); ++it) {
          cv::Rect rec(it->at(0) - padding, it->at(1) - padding,
                       it->at(2) - it->at(0) + 2 * padding,
                       it->at(3) - it->at(1) + 2 * padding);
          cv::Mat face = frameData(rec); // take the ROI of box on the frame
          msCmp->setField(std::to_string(computeCounter), "CMP_FINISH", 0);
          NDN_LOG_INFO("Output Queue Size: " << output->size());
          iceflow::Block resultBlock;
          resultBlock.pushJson(jsonData);
          resultBlock.pushFrame(face);
          auto end = std::chrono::system_clock::now();
          std::chrono::duration<double> elapsedTime = (end - start);
          NDN_LOG_INFO("Face Detection Compute Time: " << elapsedTime.count());

          auto blockingTimeStart = std::chrono::system_clock::now();
          output->pushData(resultBlock, outputThreshold);
          NDN_LOG_INFO("Output Queue Size: " << output->size());
          auto blockingTimeEnd = std::chrono::system_clock::now();

          std::chrono::duration<double> elapsedBlockingTime =
              (blockingTimeEnd - blockingTimeStart);
          NDN_LOG_INFO("Blocking Time: " << elapsedBlockingTime.count());
          NDN_LOG_INFO("Absolute Compute time: "
                       << (elapsedTime - elapsedBlockingTime).count());

          // ##### MEASUREMENT #####
          msCmp->setField(std::to_string(inputJson["frameID"].get<int>()),
                          "FD->AD", 0);
          msCmp->setField(std::to_string(inputJson["frameID"].get<int>()),
                          "FD->GD", 0);
          msCmp->setField(std::to_string(inputJson["frameID"].get<int>()),
                          "FD->ED", 0);
        }

        computeCounter++;
      }
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

[[noreturn]] void
fusion(std::vector<iceflow::RingBuffer<iceflow::Block> *> *inputs,
       iceflow::RingBuffer<iceflow::Block> *totalInput, int inputThreshold) {
  while (true) {
    if (!inputs->empty() && totalInput->size() < 5) {
      for (auto &input : *inputs) {
        auto frameFg = input->waitAndPopValue();
        frameFg.printInfo();
        totalInput->push(frameFg);
      }
    }
  }
}

void startProcessing(std::string &subSyncPrefix, std::vector<int> sub,
                     std::string &subPrefixDataMain, std::string &subPrefixAck,
                     int inputThreshold, std::string &pubSyncPrefix,
                     std::string &userPrefixDataMain,
                     const std::string &userPrefixDataManifest,
                     const std::string &userPrefixAck, int nDataStreams,
                     int publishInterval, int publishIntervalNew,
                     int namesInManifest, int outputThreshold, int mapThreshold,
                     const std::string &ml_proto, const std::string &ml_model) {
  std::vector<iceflow::RingBuffer<iceflow::Block> *> inputs;
  iceflow::RingBuffer<iceflow::Block> totalInput;
  // Data
  auto *simpleProducer = new iceflow::ProducerTlv(
      pubSyncPrefix, userPrefixDataMain, userPrefixDataManifest, userPrefixAck,
      nDataStreams, publishInterval, publishIntervalNew, mapThreshold);

  auto *simpleConsumer = new iceflow::ConsumerTlv(
      subSyncPrefix, subPrefixDataMain, subPrefixAck, sub, inputThreshold);

  auto *compute = new FaceDetector();

  inputs.push_back(simpleConsumer->getInputBlockQueue());

  // Data
  std::thread th1(&iceflow::ConsumerTlv::runCon, simpleConsumer);
  std::thread th2(&fusion, &inputs, &totalInput, inputThreshold);

  std::thread th3(&FaceDetector::compute, compute, &totalInput,
                  &simpleProducer->outputQueueBlock, outputThreshold,
                  std::ref(ml_proto), std::ref(ml_model));

  std::thread th4(&iceflow::ProducerTlv::runPro, simpleProducer);
  std::vector<std::thread> ProducerThreads;
  ProducerThreads.push_back(std::move(th1));
  NDN_LOG_INFO("Thread " << ProducerThreads.size() << " Started");
  ProducerThreads.push_back(std::move(th2));
  NDN_LOG_INFO("Thread " << ProducerThreads.size() << " Started");
  ProducerThreads.push_back(std::move(th3));
  NDN_LOG_INFO("Thread " << ProducerThreads.size() << " Started");
  ProducerThreads.push_back(std::move(th4));
  NDN_LOG_INFO("Thread " << ProducerThreads.size() << " Started");

  for (auto &t : ProducerThreads) {
    t.join();
  }
}

int main(int argc, char *argv[]) {

  if (argc != 5) {
    std::cout << "usage: " << argv[0] << " "
              << "<config-file><test-name><protobuf_binary><ML-Model>"
              << std::endl;
    return 1;
  }

  YAML::Node config = YAML::LoadFile(argv[1]);
  auto consumerConfig = config["Consumer"];
  auto producerConfig = config["Producer"];
  auto measurementConfig = config["Measurement"];

  // ----------------------- Consumer------------------------------------------
  auto subSyncPrefix = consumerConfig["subSyncPrefix"].as<std::string>();
  auto subPrefixDataMain =
      consumerConfig["subPrefixDataMain"].as<std::string>();
  auto subPrefixDataManifest =
      consumerConfig["subPrefixDataManifest"].as<std::string>();
  auto subPrefixAck = consumerConfig["subPrefixAck"].as<std::string>();
  auto nSub = consumerConfig["nSub"].as<std::vector<int>>();
  int inputThreshold = consumerConfig["inputThreshold"].as<int>();

  // ----------------------- Producer -----------------------------------------

  auto pubSyncPrefix = producerConfig["pubSyncPrefix"].as<std::string>();
  auto userPrefixDataMain =
      producerConfig["userPrefixDataMain"].as<std::string>();
  auto userPrefixDataManifest =
      producerConfig["userPrefixDataManifest"].as<std::string>();
  auto userPrefixAck = producerConfig["userPrefixAck"].as<std::string>();
  int nDataStreams = producerConfig["nDataStreams"].as<int>();
  int publishInterval = producerConfig["publishInterval"].as<int>();
  int publishIntervalNew = producerConfig["publishIntervalNew"].as<int>();
  int outputThreshold = producerConfig["outputThreshold"].as<int>();
  int namesInManifest = producerConfig["namesInManifest"].as<int>();
  int mapThreshold = producerConfig["mapThreshold"].as<int>();
  std::string ml_proto = argv[3];
  std::string ml_model = argv[4];

  // --------------------------------------------------------------------------
  // ##### MEASUREMENT #####

  std::string nodeName = measurementConfig["nodeName"].as<std::string>();
  int saveInterval = measurementConfig["saveInterval"].as<int>();
  std::string measurementName = argv[2];
  ::signal(SIGINT, signalCallbackHandler);
  msCmp =
      new iceflow::Measurement(measurementName, nodeName, saveInterval, "A");

  try {
    startProcessing(subSyncPrefix, nSub, subPrefixDataMain, subPrefixAck,
                    inputThreshold, pubSyncPrefix, userPrefixDataMain,
                    userPrefixDataManifest, userPrefixAck, nDataStreams,
                    publishInterval, publishIntervalNew, namesInManifest,
                    outputThreshold, mapThreshold, ml_proto, ml_model);
  }

  catch (const std::exception &e) {
    NDN_LOG_ERROR(e.what());
  }
}
