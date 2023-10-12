/*
 * Copyright 2023 The IceFlow Authors.
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

#ifndef ICEFLOW_CORE_CONTENT_TYPES_HPP
#define ICEFLOW_CORE_CONTENT_TYPES_HPP

#include <cstdint>

namespace iceflow {

// TODO: Improve documentation
// TODO: Check if UpdateManifest and FrameManifest are using the correct numbers

/**
 * @brief IceFlow-specific ContentType values.
 */
enum ContentTypeValue : uint32_t {
  UpdateManifest = 128,
  FrameManifest = 129,
  Manifest = 130, ///<  Manifest is the collection of names of the data objects
                  ///<  (previously Main Data)
  Json = 131, ///< JSON carries the analyzed results of the compute function of
              ///< the upstreams
  JsonManifest = 141,       ///< JSON Manifest
  SegmentsInManifest = 142, ///< Segment Manifest
};

} // namespace iceflow

#endif // ICEFLOW_CORE_CONTENT_TYPES_HPP
