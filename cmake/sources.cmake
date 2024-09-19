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
file(GLOB ALL_SOURCES
  include/iceflow/*.[chCH][pP][pP] include/iceflow/*.[chCH][xX][xX]
  include/iceflow/*.[cC][cC] include/iceflow/*.[hH][hH] include/iceflow/*.[CHch]
  src/*.[chCH][pP][pP] src/*.[chCH][xX][xX] src/*.[cC][cC]
  src/*.[hH][hH] src/*.[CHch]
  src/services/*.[chCH][pP][pP] src/services/*.[chCH][xX][xX] src/services/*.[cC][cC]
  src/services/*.[hH][hH] src/services/*.[CHch]
  examples/wordCount/lines2words/*.[chCH][pP][pP] examples/wordCount/lines2words/*.[chCH][xX][xX]
  examples/wordCount/lines2words/*.[cC][cC] examples/wordCount/lines2words/*.[hH][hH]
  examples/wordCount/lines2words/*.[CHch]
  examples/wordCount/text2lines/*.[chCH][pP][pP] examples/wordCount/text2lines/*.[chCH][xX][xX]
  examples/wordCount/text2lines/*.[cC][cC] examples/wordCount/text2lines/*.[hH][hH]
  examples/wordCount/text2lines/*.[CHch]
  examples/wordCount/wordcount/*.[chCH][pP][pP] examples/wordCount/wordcount/*.[chCH][xX][xX]
  examples/wordCount/wordcount/*.[cC][cC] examples/wordCount/wordcount/*.[hH][hH]
  examples/wordCount/wordcount/*.[CHch]
  examples/visionProcessing/imageSource/*.[chCH][pP][pP]
  examples/visionProcessing/faceDetection/*.[chCH][pP][pP]
  examples/visionProcessing/ageDetection/*.[chCH][pP][pP]
  examples/visionProcessing/genderDetection/*.[chCH][pP][pP]
  examples/visionProcessing/aggregate/*.[chCH][pP][pP]
  examples/visionProcessing/peopleCount/*.[chCH][pP][pP]
)
