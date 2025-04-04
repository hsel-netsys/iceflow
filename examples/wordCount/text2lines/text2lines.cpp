#include "iceflow/dag-parser.hpp"
#include "iceflow/iceflow.hpp"
#include "iceflow/measurements.hpp"
#include "iceflow/producer.hpp"

#ifdef USE_GRPC
#include "iceflow/scaler.hpp"
#endif

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <csignal>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#include <yaml-cpp/yaml.h>

iceflow::Measurement *measurementHandler;

void signalCallbackHandler(int signum) {
  measurementHandler->recordToFile();
  exit(signum);
}

class LineSplitter {
public:
  void text2lines(const std::string &fileName,
                  std::function<void(std::string)> push) {

    std::ifstream file;
    file.open(fileName);

    if (file.is_open()) {
      int computeCounter = 0;

      for (std::string line; getline(file, line);) {
        measurementHandler->setField(std::to_string(computeCounter),
                                     "CMP_START", 0);
        push(line);
        measurementHandler->setField(std::to_string(computeCounter),
                                     "text->lines1", 0);
        measurementHandler->setField(std::to_string(computeCounter),
                                     "text->lines2", 0);
        measurementHandler->setField(std::to_string(computeCounter),
                                     "CMP_FINISH", 0);
        computeCounter++;
      }
      file.close();
      measurementHandler->setField(std::to_string(computeCounter), "CMP_FINISH",
                                   0);
    } else {
      std::cerr << "Error opening file: " << fileName << std::endl;
    }
  }
};

void run(const std::string &nodeName, const std::string &dagFileName) {
  std::cout << "Starting IceFlow Stream Processing - - - -" << std::endl;
  LineSplitter lineSplitter;
  ndn::Face face;

  auto iceflow =
      std::make_shared<iceflow::IceFlow>(dagFileName, nodeName, face);
  auto downstreamEdgeName = iceflow->getDownstreamEdge(0).value().id;

  auto saveThreshold =
      iceflow->getApplicationParameter<uint64_t>("measurementsSaveThreshold")
          .value();
  auto sourceTextFileName =
      iceflow->getApplicationParameter<std::string>("sourceTextFileName")
          .value();

  ::signal(SIGINT, signalCallbackHandler);
  measurementHandler = new iceflow::Measurement(
      nodeName, iceflow->getNodePrefix(), saveThreshold, "A");

  std::vector<std::thread> threads;
  threads.emplace_back(&iceflow::IceFlow::run, iceflow);
  threads.emplace_back([&lineSplitter, &iceflow, &sourceTextFileName,
                        &downstreamEdgeName]() {
    lineSplitter.text2lines(sourceTextFileName, [&iceflow, &downstreamEdgeName](
                                                    const std::string &data) {
      std::vector<uint8_t> encodedString(data.begin(), data.end());
      iceflow->pushData(downstreamEdgeName, encodedString);
    });
  });

#ifdef USE_GRPC
  auto grpcPath =
      iceflow->getApplicationParameter<std::string>("grpcPath").value();
  auto scaler =
      iceflow::IceflowScaler(grpcPath + "/node-instance.sock",
                             grpcPath + "/node-executor.sock", iceflow);
#endif

  for (auto &thread : threads) {
    thread.join();
  }
}

int main(int argc, const char *argv[]) {

  if (argc != 2) {
    std::string command = argv[0];
    std::cout << "usage: " << command << " <application-dag-file>" << std::endl;
    return 1;
  }

  try {
    std::string nodeName = "text2lines";
    std::string dagFileName = argv[1];

    run(nodeName, dagFileName);
  } catch (const std::exception &e) {
    std::cout << e.what() << std::endl;
  }

  return 0;
}
