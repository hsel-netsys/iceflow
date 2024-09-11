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

#ifndef ICEFLOW_CONGESTION_REPORTER_HPP
#define ICEFLOW_CONGESTION_REPORTER_HPP

#ifdef USE_GRPC

#include "node-executor.pb.h"

#else // !USE_GRPC

enum CongestionReason : int {
  CONGESTION_REASON_NETWORK = 0,
  CONGESTION_REASON_PROCESSING = 1
};

#endif // USE_GRPC

namespace iceflow {

class CongestionReporter {
public:
  virtual ~CongestionReporter() {}

  virtual void reportCongestion(CongestionReason congestionReason,
                                const std::string &edgeName) = 0;
};

} // namespace iceflow

#endif // ICEFLOW_CONGESTION_REPORTER_HPP
