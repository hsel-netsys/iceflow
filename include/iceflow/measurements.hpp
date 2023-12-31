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

#include <fstream>

#include "logger.hpp"

namespace iceflow {

struct Entry // represents an entry: #, entryname, timestamp
{
  // string nodeName;
  // string observedObject;
  std::string interest;
  std::string entryname;
  uint64_t timestamp;
  unsigned short size;
};

class Measurement {
public:
  Measurement(const std::string &measurementId, const std::string &nodeName,
              int saveInterval, const std::string &observedObject)
      : m_observedObject(observedObject), m_nodeName(nodeName),
        m_measurementId(measurementId), m_saveInterval(saveInterval) {
    m_fileCount = 0;
    m_lastSaveToFile = duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
  }
  ~Measurement() {}
  void setField(const std::string &interestName, const std::string &entryName,
                int dataSize) {
    NDN_LOG_DEBUG("################# SETTING FIELD "
                  << interestName << " - " << entryName << "#################");
    Entry entry;
    entry.interest = interestName;
    entry.entryname = entryName;
    entry.size = dataSize;
    uint64_t currentTimestamp =
        duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    entry.timestamp = currentTimestamp;
    m_entries.push_back(entry);
    if (m_entries.size() >= m_saveInterval) {
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
  int m_saveInterval;
  std::ofstream m_ofstream;
  int m_lastSaveToFile;
};

} // namespace iceflow

#endif // ICEFLOW_CORE_MEASUREMENTS_H
