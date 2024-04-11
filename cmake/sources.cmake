# Copyright 2024 The IceFlow Authors.
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

# Find all source files for formatting, see the replies to https://stackoverflow.com/a/36046965
# and https://www.labri.fr/perso/fleury/posts/programming/using-clang-tidy-and-clang-format.html.
file(GLOB_RECURSE ALL_SOURCES
  examples/*.[chCH][pP][pP] examples/*.[chCH][xX][xX] examples/*.[cC][cC]
  examples/*.[hH][hH] examples/*.[CHch]
  include/*.[chCH][pP][pP] include/*.[chCH][xX][xX] include/*.[cC][cC]
  include/*.[hH][hH] include/*.[CHch]
)
