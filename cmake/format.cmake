# Copyright 2023 The IceFlow Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#
# SPDX-License-Identifier: Apache-2.0

# This file introduces custom `format` and `check-format` targets which simplify
# the process of formatting the project's source code as well as verifying it.
#
# After running `cmake .`, you can automatically format the codebase using `make
# format`. To only check whether the codebase is properly formatted, run `make
# check-format` instead.

include("cmake/sources.cmake")

add_custom_target(format)
add_custom_command(
  TARGET format
  COMMAND clang-format -i ${ALL_SOURCES}
  COMMENT "Formatting codebase...")

add_custom_target(check-format)
add_custom_command(
  TARGET check-format
  COMMAND clang-format -i ${ALL_SOURCES} --dry-run -Werror
  COMMENT "Verifying formatting...")
