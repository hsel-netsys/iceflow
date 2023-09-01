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

# This file introduces a custom `link` target which simplifies the process of
# running cppcheck for linting the project's source code.

# TODO: Create one variable that is also used for formatting sources
set(LINTING_SOURCES apps/**/*.cpp tests/*.cpp include/**/*.hpp)

add_custom_target(lint)
add_custom_command(TARGET lint COMMAND cppcheck --enable=all ${LINTING_SOURCES})
