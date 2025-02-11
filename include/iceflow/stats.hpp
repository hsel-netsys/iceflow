/*
 * Copyright 2025 The IceFlow Authors.
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

#ifndef ICEFLOW_STATS_HPP
#define ICEFLOW_STATS_HPP

#include <optional>

namespace iceflow {

struct EdgeProductionStats {
  uint64_t unitsProduced;
  uint64_t idleTime;
};

struct EdgeConsumptionStats {
  uint64_t unitsConsumed;
  uint64_t idleTime;
};

struct EdgeStats {
  std::optional<EdgeProductionStats> productionStats;
  std::optional<EdgeConsumptionStats> consumptionStats;
};

} // namespace iceflow

#endif // ICEFLOW_STATS_HPP
