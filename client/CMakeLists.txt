# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2020 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 2.8)
project(rdkshellclient)
set(CMAKE_CXX_STANDARD 14)

option(RDKSHELLCLIENT_BUILD_TEST_APP "RDKSHELLCLIENT_BUILD_TEST_APP" ON)

set(RDKSHELLDIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(COMMUNICATIONDIR ${CMAKE_CURRENT_SOURCE_DIR}/../communication)

set(RDKSHELLCLIENT_SOURCES rdkshellclient.cpp clientmessagehandler.cpp ${COMMUNICATIONDIR}/socket/sockethandler.cpp ${COMMUNICATIONDIR}/communicationfactory.cpp ${COMMUNICATIONDIR}/communicationutils.cpp)
set(RDKSHELLCLIENT_ADDITIONAL_SOURCES ${RDKSHELLDIR}/rdkshelldata.cpp ${RDKSHELLDIR}/logger.cpp)
set(RDKSHELLCLIENT_SOURCES ${RDKSHELLCLIENT_SOURCES} ${RDKSHELLCLIENT_ADDITIONAL_SOURCES})

set(RDKSHELLCLIENT_LINK_LIBRARIES -lz)

add_library(rdkshellclient_shared SHARED ${RDKSHELLCLIENT_SOURCES})
set_target_properties(rdkshellclient_shared PROPERTIES OUTPUT_NAME rdkshellclient)
target_link_libraries(rdkshellclient_shared ${RDKSHELLCLIENT_LINK_LIBRARIES})
include_directories(AFTER ${CMAKE_CURRENT_SOURCE_DIR} ${COMMUNICATIONDIR} ${COMMUNICATIONDIR}/socket ${RDKSHELLDIR}) 

if (RDKSHELLCLIENT_BUILD_TEST_APP)
    message("Building rdkshell test client")
    add_executable(rdkshell_test test/test_rdkshellclient.cpp)
    add_dependencies(rdkshell_test rdkshellclient_shared)
    target_link_libraries(rdkshell_test ${RDKSHELLCLIENT_LINK_LIBRARIES} rdkshellclient_shared)
endif (RDKSHELLCLIENT_BUILD_TEST_APP)
