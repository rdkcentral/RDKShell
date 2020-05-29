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
project(shell)
set(CMAKE_CXX_STANDARD 14)

option(RDKSHELL_BUILD_APP "RDKSHELL_BUILD_APP" ON)
option(RDKSHELL_SUPPORT_WEBSOCKET_IPC "RDKSHELL_SUPPORT_WEBSOCKET_IPC" OFF)

set(RDKSHELL_SOURCES rdkshell.cpp compositorcontroller.cpp essosinstance.cpp rdkcompositor.cpp linuxkeys.cpp)
set(RDKSHELL_LINK_LIBRARIES -lz -lessos -lEGL -lGLESv2 -lwayland-client -lwesteros_compositor)

if (RDKSHELL_SUPPORT_WEBSOCKET_IPC)
  set(RDKSHELL_SOURCES ${RDKSHELL_SOURCES} messageHandler.cpp)
  set(RDKSHELL_LINK_LIBRARIES ${RDKSHELL_LINK_LIBRARIES} -luWS)
  add_definitions("-DRDKSHELL_ENABLE_WEBSOCKET_IPC")
endif (RDKSHELL_SUPPORT_WEBSOCKET_IPC)

add_library(rdkshell_shared SHARED ${RDKSHELL_SOURCES})
set_target_properties(rdkshell_shared PROPERTIES OUTPUT_NAME rdkshell)
target_link_libraries(rdkshell_shared ${RDKSHELL_LINK_LIBRARIES})


if (RDKSHELL_BUILD_APP)
    message("Building rdkshell executable")
    add_executable(rdkshell main.cpp)
    add_dependencies(rdkshell rdkshell_shared)
    target_link_libraries(rdkshell ${RDKSHELL_LINK_LIBRARIES} rdkshell_shared)
endif (RDKSHELL_BUILD_APP)
