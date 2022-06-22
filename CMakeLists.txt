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
option(RDKSHELL_BUILD_WEBSOCKET_IPC "RDKSHELL_BUILD_WEBSOCKET_IPC" OFF)
option(RDKSHELL_BUILD_KEY_METADATA "RDKSHELL_BUILD_KEY_METADATA" OFF)
option(RDKSHELL_BUILD_KEY_METADATA_EXTENDED_SUPPORT_FOR_IR "RDKSHELL_BUILD_KEY_METADATA_EXTENDED_SUPPORT_FOR_IR" OFF)
option(RDKSHELL_BUILD_IPC "RDKSHELL_BUILD_IPC" ON)
option(RDKSHELL_BUILD_CLIENT "RDKSHELL_BUILD_CLIENT" ON)
option(RDKSHELL_BUILD_HIDDEN_SUPPORT "RDKSHELL_BUILD_HIDDEN_SUPPORT" OFF)
option(RDKSHELL_BUILD_FORCE_1080 "RDKSHELL_BUILD_FORCE_1080" OFF)
option(RDKSHELL_BUILD_FORCE_ANIMATE "RDKSHELL_BUILD_FORCE_ANIMATE" OFF)
option(RDKSHELL_BUILD_CLIENT_CONTROL_EXTENSION_TEST "RDKSHELL_BUILD_CLIENT_CONTROL_EXTENSION_TEST" OFF)
option(RDKSHELL_BUILD_EXTERNAL_APPLICATION_SURFACE_COMPOSITION "RDKSHELL_BUILD_EXTERNAL_APPLICATION_SURFACE_COMPOSITION" ON)
option(RDKSHELL_BUILD_KEYBUBBING_TOP_MODE "RDKSHELL_BUILD_KEYBUBBING_TOP_MODE" ON)
option(RDKSHELL_BUILD_ENABLE_KEYREPEATS "RDKSHELL_BUILD_ENABLE_KEYREPEATS" OFF)


set(COMMUNICATIONDIR ${CMAKE_CURRENT_SOURCE_DIR}/communication)

set(RDKSHELL_SOURCES
  rdkshell.cpp
  compositorcontroller.cpp
  essosinstance.cpp
  rdkcompositor.cpp
  rdkcompositorsurface.cpp
  rdkcompositornested.cpp
  linuxkeys.cpp
  eastereggs.cpp
  animation.cpp
  animationutilities.cpp
  rdkshelldata.cpp
  rdkshelljson.cpp
  linuxinput.cpp
  logger.cpp
  rdkshellimage.cpp
  permissions.cpp
  framebuffer.cpp
  framebufferrenderer.cpp
  cursor.cpp
)
set(RDKSHELL_LINK_LIBRARIES -lz -lessos -lEGL -lGLESv2 -lwayland-client -lwesteros_compositor -lpthread -ljpeg -lpng16)

if (RDKSHELL_BUILD_WEBSOCKET_IPC)
  set(RDKSHELL_SOURCES ${RDKSHELL_SOURCES} messageHandler.cpp)
  set(RDKSHELL_LINK_LIBRARIES ${RDKSHELL_LINK_LIBRARIES} -luWS)
  add_definitions("-DRDKSHELL_ENABLE_WEBSOCKET_IPC")
endif (RDKSHELL_BUILD_WEBSOCKET_IPC)

if (RDKSHELL_BUILD_IPC)
  message("Building rdkshell ipc")
  include_directories(AFTER ${CMAKE_CURRENT_SOURCE_DIR} ${COMMUNICATIONDIR} ${COMMUNICATIONDIR}/socket)
  set(RDKSHELL_SOURCES ${RDKSHELL_SOURCES} servermessagehandler.cpp ${COMMUNICATIONDIR}/socket/sockethandler.cpp ${COMMUNICATIONDIR}/communicationfactory.cpp ${COMMUNICATIONDIR}/communicationutils.cpp)
  set(RDKSHELL_LINK_LIBRARIES ${RDKSHELL_LINK_LIBRARIES})
  add_definitions("-DRDKSHELL_ENABLE_IPC")
endif (RDKSHELL_BUILD_IPC)

if (RDKSHELL_WESTEROS_PLUGIN_FOLDER)
    add_definitions(-DRDKSHELL_WESTEROS_PLUGIN_DIRECTORY="${RDKSHELL_WESTEROS_PLUGIN_FOLDER}")
else()
    add_definitions(-DRDKSHELL_WESTEROS_PLUGIN_DIRECTORY="/usr/lib/plugins/westeros/")
endif (RDKSHELL_WESTEROS_PLUGIN_FOLDER)

if (RDKSHELL_BUILD_KEY_METADATA)
  add_definitions("-DRDKSHELL_ENABLE_KEY_METADATA")
endif (RDKSHELL_BUILD_KEY_METADATA)

if (RDKSHELL_BUILD_HIDDEN_SUPPORT)
  add_definitions("-DRDKSHELL_ENABLE_HIDDEN_SUPPORT")
endif (RDKSHELL_BUILD_HIDDEN_SUPPORT)

if (RDKSHELL_BUILD_FORCE_1080)
  add_definitions("-DRDKSHELL_ENABLE_FORCE_1080")
endif (RDKSHELL_BUILD_FORCE_1080)

if (RDKSHELL_BUILD_FORCE_ANIMATE)
  add_definitions("-DRDKSHELL_ENABLE_FORCE_ANIMATE")
endif (RDKSHELL_BUILD_FORCE_ANIMATE)

# This setting requires a westeros/essos that has extended support for IR
if (RDKSHELL_BUILD_KEY_METADATA_EXTENDED_SUPPORT_FOR_IR)
  if (NOT RDKSHELL_BUILD_KEY_METADATA)
    message(WARNING "setting RDKSHELL_BUILD_KEY_METADATA_EXTENDED_SUPPORT_FOR_IR has no effect without RDKSHELL_BUILD_KEY_METADATA")
  endif (NOT RDKSHELL_BUILD_KEY_METADATA)

  add_definitions("-DRDKSHELL_ENABLE_KEY_METADATA_EXTENDED_SUPPORT_FOR_IR")
endif (RDKSHELL_BUILD_KEY_METADATA_EXTENDED_SUPPORT_FOR_IR)

if (RDKSHELL_BUILD_EXTERNAL_APPLICATION_SURFACE_COMPOSITION)
  add_definitions("-DRDKSHELL_ENABLE_EXTERNAL_APPLICATION_SURFACE_COMPOSITION")
endif (RDKSHELL_BUILD_EXTERNAL_APPLICATION_SURFACE_COMPOSITION)

if (RDKSHELL_BUILD_KEYBUBBING_TOP_MODE)
  add_definitions("-DRDKSHELL_ENABLE_KEYBUBBING_TOP_MODE")
endif (RDKSHELL_BUILD_KEYBUBBING_TOP_MODE)

if (RDKSHELL_BUILD_ENABLE_KEYREPEATS)
  add_definitions("-DRDKSHELL_ENABLE_KEYREPEATS")
endif (RDKSHELL_BUILD_ENABLE_KEYREPEATS)

if(BUILD_ENABLE_ERM)
    add_definitions("-DENABLE_ERM")
    set(RDKSHELL_LINK_LIBRARIES ${RDKSHELL_LINK_LIBRARIES} -lessosrmgr)
endif()

add_subdirectory(extensions/RdkShellExtendedInput)
add_subdirectory(extensions/RdkShellClientControl)
add_library(rdkshell_shared SHARED ${RDKSHELL_SOURCES})
set_target_properties(rdkshell_shared PROPERTIES OUTPUT_NAME rdkshell)
target_link_libraries(rdkshell_shared ${RDKSHELL_LINK_LIBRARIES})


if (RDKSHELL_BUILD_APP)
    message("Building rdkshell executable")
    add_executable(rdkshell main.cpp)
    add_dependencies(rdkshell rdkshell_shared)
    target_link_libraries(rdkshell ${RDKSHELL_LINK_LIBRARIES} rdkshell_shared -lpthread)
endif (RDKSHELL_BUILD_APP)

if (RDKSHELL_BUILD_CLIENT)
    message("Building rdkshell client")
    add_subdirectory(client)
endif (RDKSHELL_BUILD_CLIENT)

if (RDKSHELL_BUILD_CLIENT_CONTROL_EXTENSION_TEST)
    message("Building rdkshell client control extension test")
    add_subdirectory(tests/ClientControlExtension)
endif (RDKSHELL_BUILD_CLIENT_CONTROL_EXTENSION_TEST)
