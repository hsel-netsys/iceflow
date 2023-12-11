#include "iceflow/Producer.hpp"
#include "iceflow/measurements.hpp"

#include <csignal>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#include <yaml-cpp/yaml.h>

// ###### MEASUREMENT ######
iceflow::Measurement *msCmp;

void signalCallbackHandler(int signum) {
  msCmp->recordToFile();
  // Terminate program
  exit(signum);
}

class Compute {
public:
  void text2lines(const std::string &filename,
                  std::function<void(std::string)> push) {

    auto application = applicationName();
    std::cout << "Starting " << application << " Application - - - - "
              << std::endl;
    int computeCounter = 0;
    std::ifstream file;
    file.open(filename);

    if (file.is_open()) {
      for (std::string line; getline(file, line);) {
        //				std::cout << line + ";" << std::endl;
        // ##### MEASUREMENT #####
        msCmp->setField(std::to_string(computeCounter), "CMP_START", 0);
        push(line);
        msCmp->setField(std::to_string(computeCounter), "text->lines1", 0);
        msCmp->setField(std::to_string(computeCounter), "text->lines2", 0);
        msCmp->setField(std::to_string(computeCounter), "CMP_FINISH", 0);
        computeCounter++;
      }
      file.close();
      msCmp->setField(std::to_string(computeCounter), "CMP_FINISH", 0);
    } else {
      std::cerr << "Error opening file: " << filename << std::endl;
    }
  }

  std::string applicationName() {
    const char *fullPath = __FILE__;
    const char *fileNameWithExtension =
        strrchr(fullPath, '/'); // For Unix-like paths
    // const char* fileNameWithExtension = strrchr(fullPath, '\\'); // For
    // Windows paths

    if (fileNameWithExtension == nullptr) {
      // If no directory separator is found, use the whole path
      fileNameWithExtension = fullPath;
    } else {
      // Move one character ahead to exclude the directory separator
      fileNameWithExtension++;
    }

    const char *fileName = strrchr(fileNameWithExtension, '.');

    if (fileName != nullptr) {
      // If a dot (.) is found, truncate the string at that position
      size_t length = fileName - fileNameWithExtension;
      char appName[length + 1];
      strncpy(appName, fileNameWithExtension, length);
      appName[length] = '\0';
      return appName;
    }
  }
};

void DataFlow(const std::string &pub_syncPrefix,
              const std::string &userPrefix_data_main,
              std::vector<int> &nDataStreams, const std::string &filename) {
  std::cout << "Starting IceFlow Stream Processing - - - -" << std::endl;
  Compute compute;
  ndn::Face interFace;

  iceflow::Producer producer(pub_syncPrefix, userPrefix_data_main, nDataStreams,
                             interFace);
  std::vector<std::thread> ProducerThreads;
  ProducerThreads.emplace_back(&iceflow::Producer::run, &producer);
  ProducerThreads.emplace_back([&compute, &producer, &filename]() {
    compute.text2lines(filename,
                       [&producer](std::string data) { producer.push(data); });
  });

  for (auto &t : ProducerThreads) {
    t.join();
  }
}

int main(int argc, char *argv[]) {
  if (argc != 4) {
    std::cout << "usage: " << argv[0] << " "
              << "<config-file><text-file><measurement-Name>" << std::endl;
    return 1;
  }

  YAML::Node config = YAML::LoadFile(argv[1]);

  auto pubsyncPrefix = config["Producer"]["pubsyncPrefix"].as<std::string>();
  auto pubPrefixdatamain =
      config["Producer"]["pubPrefixdatamain"].as<std::string>();
  auto nPartition = config["Producer"]["nPartition"].as<std::vector<int>>();

  std::string filename = argv[2];

  // ##### MEASUREMENT #####
  std::string measurementFileName = argv[3];
  auto measurementConfig = config["Measurement"];
  std::string nodeName = measurementConfig["nodeName"].as<std::string>();
  int saveInterval = measurementConfig["saveInterval"].as<int>();
  ::signal(SIGINT, signalCallbackHandler);
  msCmp = new iceflow::Measurement(measurementFileName, nodeName, saveInterval,
                                   "A");

  try {

    DataFlow(pubsyncPrefix, pubPrefixdatamain, nPartition, filename);
  } catch (const std::exception &e) {
    std::cout << e.what() << std::endl;
  }

  return 0;
}
