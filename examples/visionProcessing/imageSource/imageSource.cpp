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
  void imageSource(
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

      if (grayFrame.empty()) {
        std::cerr << "Error: Received empty image data." << std::endl;
        return;
      }

      std::vector<uint8_t> encodedVideo;
      if (!cv::imencode(".jpeg", grayFrame, encodedVideo)) {
        std::cerr << "Error: Could not encode image to JPEG format."
                  << std::endl;
        return;
      }

      nlohmann::json resultData;
      resultData["frameID"] = frameID;
      resultData["image"] = nlohmann::json::binary(encodedVideo);

      auto serializedData = Serde::serialize(resultData);
      std::cout << "ImageSource:\n"
                << "FrameID:" << frameID << "\n"
                << "Encoded Image Size: " << serializedData.size() << " bytes"
                << std::endl;
      std::cout << "------------------------------------" << std::endl;

      push(serializedData, "is2fd");
      push(serializedData, "is2pc");

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
    std::string downstreamEdgeNameFD, downstreamEdgeNamePC;
    for (const auto &downstream : node.downstream) {
      if (downstream.id == "is2fd") {
        downstreamEdgeNameFD = downstream.id;
      } else if (downstream.id == "is2pc") {
        downstreamEdgeNamePC = downstream.id;
      }
    }

    if (downstreamEdgeNameFD.empty() || downstreamEdgeNamePC.empty()) {
      std::cerr << "Error: Missing downstream targets for face detection or "
                   "people counting."
                << std::endl;
      return;
    }

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
                          downstreamEdgeNameFD, downstreamEdgeNamePC]() {
      inputImage.imageSource(videoFile,
                             [&iceflow](const std::vector<uint8_t> &data,
                                        const std::string &edgeName) {
                               iceflow->pushData(edgeName, data);
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
