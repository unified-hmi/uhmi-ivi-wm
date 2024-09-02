# SPDX-License-Identifier: Apache-2.0
#
# Copyright (c) 2024  Panasonic Automotive Systems, Co., Ltd.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

SET(SRC_FILES
  main.c
  comm_parser.c
  comm_receiver.c
  ilm_control_wrapper.c
)
add_executable(${PROJECT_NAME} ${SRC_FILES})

SET(LIBS
  ilmCommon
  ilmControl
  -lpthread
  jansson
)
target_link_libraries(${PROJECT_NAME} ${LIBS})
install (TARGETS ${PROJECT_NAME} DESTINATION bin)
