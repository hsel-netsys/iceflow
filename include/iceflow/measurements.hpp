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

#ifndef ICEFLOW_CORE_MEASUREMENTS_H
#define ICEFLOW_CORE_MEASUREMENTS_H

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "logger.hpp"

namespace iceflow {

struct Entry {
  std::string interest;
  std::string entryname;
  uint64_t timestamp;
  uint64_t size;
};

class Measurement {
public:
  Measurement(const std::string &measurementId, const std::string &nodeName,
              uint64_t saveThreshold, const std::string &observedObject)
      : m_observedObject(observedObject), m_nodeName(nodeName),
        m_measurementId(measurementId), m_saveThreshold(saveThreshold) {
    m_fileCount = 0;
    createMeasurementFolder();
  }
  ~Measurement(){};

  void createMeasurementFolder() {
    std::string folderName = "measurements";

    // Combine the current working directory with the folder name
    std::filesystem::path folderPath =
        std::filesystem::current_path() / folderName;

    // Check if the folder already exists
    if (!std::filesystem::exists(folderPath)) {
      // Create the folder
      if (std::filesystem::create_directory(folderPath)) {
        std::cout << "Folder created successfully in the current directory."
                  << std::endl;
      } else {
        std::cerr << "Error creating folder." << std::endl;
      }
    } else {
      std::cout << "Folder already exists in the current directory."
                << std::endl;
    }
  }

  void setField(const std::string &interestName, const std::string &entryName,
                uint64_t dataSize) {
    NDN_LOG_DEBUG("################# SETTING FIELD "
                  << interestName << " - " << entryName << "#################");

    uint64_t currentTimestamp =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();

    Entry entry = {
        interestName,
        entryName,
        currentTimestamp,
        dataSize,
    };

    m_entries.push_back(entry);
    if (m_entries.size() >= m_saveThreshold) {
      recordToFile();
    }
  }

  void recordToFile() {
    NDN_LOG_DEBUG("################# RECORDING TO FILE #################");
    m_fileName = "measurements/" + m_measurementId + "_" + m_nodeName + "_" +
                 m_observedObject + "_part_" + std::to_string(++m_fileCount) +
                 ".csv";

    m_ofstream.open(m_fileName, std::ios::out);
    m_ofstream
        << "nodeName,observedObject,interest,entryname,timestamp,dataSize\n";
    for (const auto &e : m_entries) {
      std::ostringstream lineToAdd;
      lineToAdd << m_nodeName << ",";
      lineToAdd << m_observedObject << ",";
      lineToAdd << e.interest << ",";
      lineToAdd << e.entryname << ",";
      lineToAdd << e.timestamp << ",";
      lineToAdd << e.size << "\n";
      m_ofstream << lineToAdd.str();
      lineToAdd.str("");
      lineToAdd.clear();
    }
    m_ofstream.close();
    m_entries.clear();
    m_entries.shrink_to_fit();
    NDN_LOG_DEBUG("Saved to file and cleared the vector: " +
                  std::to_string(m_fileCount));
  }

private:
  std::string m_fileName;
  std::string m_nodeName;
  std::string m_measurementId;
  std::string m_observedObject;
  int m_fileCount;
  std::vector<Entry> m_entries;
  uint64_t m_saveThreshold;
  std::ofstream m_ofstream;
};

} // namespace iceflow

#endif // ICEFLOW_CORE_MEASUREMENTS_H
