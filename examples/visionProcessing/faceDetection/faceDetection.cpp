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

class FaceDetection {
public:
  FaceDetection(const std::string &protobufFile, const std::string &mlModel)
      : faceNet(cv::dnn::readNet(mlModel, protobufFile)) {}

  void faceDetection(std::vector<uint8_t> encodedCropped,
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
      measurementHandler->setField(std::to_string(frameID), "IS->FD", 0);

      std::tie(frameFace, bboxes) = getFaceBox(faceNet, greyImage, 0.7);

      for (const auto &box : bboxes) {
        int x1 = std::max(0, box[0] - padding);
        int y1 = std::max(0, box[1] - padding);
        int x2 = std::min(greyImage.cols, box[2] + padding);
        int y2 = std::min(greyImage.rows, box[3] + padding);

        cv::Rect faceRect(x1, y1, x2 - x1, y2 - y1);

        if (faceRect.width > 0 && faceRect.height > 0) {
          cv::Mat greyface = greyImage(faceRect);

          std::cout << "Detected face: "
                    << greyface.total() * greyface.elemSize() << " bytes"
                    << std::endl;

          // Encode the cropped face for further processing
          std::vector<uint8_t> encodedCroppedFace;
          if (!cv::imencode(".jpeg", greyface, encodedCroppedFace)) {
            std::cerr << "Error: Could not encode image." << std::endl;
            return;
          }

          nlohmann::json resultData;
          resultData["frameID"] = frameID;
          resultData["image"] = nlohmann::json::binary(encodedCroppedFace);

          std::vector<uint8_t> detectedFace = Serde::serialize(resultData);

          std::cout << "FaceDetection: \n"
                    << " FrameID:" << frameID << "\n"
                    << " Encoded Image Size: " << detectedFace.size()
                    << " bytes" << std::endl;
          std::cout << "------------------------------------" << std::endl;

          push(detectedFace);
          measurementHandler->setField(std::to_string(frameID), "FD->AD", 0);
          measurementHandler->setField(std::to_string(frameID), "FD->GD", 0);
          measurementHandler->setField(std::to_string(frameID), "FD->ED", 0);
          measurementHandler->setField(std::to_string(computeCounter),
                                       "CMP_FINISH", 0);
          computeCounter++;
        }
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
    // cv::Scalar meanVal = cv::Scalar(104, 117, 123);
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

private:
  std::vector<std::vector<int>> bboxes;
  cv::Mat frameFace;
  int padding = 20;
  int computeCounter = 0;
  cv::dnn::Net faceNet;
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

  FaceDetection facialDetection(protobufFile, mlModel);
  ndn::Face face;

  auto dagParser = iceflow::DAGParser::parseFromFile(dagFileName);
  auto node = dagParser.findNodeByName(nodeName);
  if (!node.downstream.empty()) {
    std::string downstreamEdgeNameAD, downstreamEdgeNameGD;
    for (const auto &downstream : node.downstream) {
      if (downstream.id == "fd2ad") {
        downstreamEdgeNameAD = downstream.id;
      } else if (downstream.id == "fd2gd") {
        downstreamEdgeNameGD = downstream.id;
      }
    }

    if (downstreamEdgeNameAD.empty() || downstreamEdgeNameGD.empty()) {
      std::cerr << "Error: Missing downstream targets for face detection or "
                   "people counting."
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

    auto prosumerCallback =
        [&iceflow, &facialDetection, &downstreamEdgeNameAD,
         &downstreamEdgeNameGD](const std::vector<uint8_t> &encodedCroppedFace,
                                iceflow::ProducerCallback producerCallback) {
          auto pushDataCallback =
              [downstreamEdgeNameAD, downstreamEdgeNameGD,
               producerCallback](std::vector<uint8_t> encodedCroppedFace) {
                producerCallback(downstreamEdgeNameAD, encodedCroppedFace);
                producerCallback(downstreamEdgeNameGD, encodedCroppedFace);
              };
          facialDetection.faceDetection(encodedCroppedFace, pushDataCallback);
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
    std::string nodeName = "faceDetection";
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
