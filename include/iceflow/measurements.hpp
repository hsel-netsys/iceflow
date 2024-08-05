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
              uint64_t saveThreshold, const std::string &observedObject);
  ~Measurement();

  void createMeasurementFolder();

  void setField(const std::string &interestName, const std::string &entryName,
                uint64_t dataSize);

  void recordToFile();

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
