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

#ifndef ICEFLOW_APPS_UTIL_HPP
#define ICEFLOW_APPS_UTIL_HPP

#include "iceflow/block.hpp"
#include "opencv2/opencv.hpp"

void pushFrame(iceflow::Block *block, cv::Mat frame);

void pushFrameCompress(iceflow::Block *block, cv::Mat frame);

cv::Mat pullFrame(iceflow::Block block);

#endif // ICEFLOW_APPS_UTIL_HPP
