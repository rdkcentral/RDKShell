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

add_library(RdkShellShell 
        SHARED
        source/rdkshellshell.cpp
        protocol/rdkshell_shell_protocol.c
        )

#target_link_libraries(RdkShellShell rdkshell_shared)

add_dependencies(RdkShellRenderer RdkShellLink)

set(RDKSHELLRENDERER_LINK_LIBRARIES  -L${CMAKE_CURRENT_SOURCE_DIR}/../../../build -L${CMAKE_CURRENT_BINARY_DIR}/../RdkShellLink/ -lrdkshell_link)
target_link_libraries(RdkShellShell ${RDKSHELLRENDERER_LINK_LIBRARIES} rdkshell_shared)


target_include_directories(RdkShellShell
        PRIVATE
        protocol
        $<TARGET_PROPERTY:rdkshell_shared,INTERFACE_INCLUDE_DIRECTORIES>
        )

set_target_properties(RdkShellShell
        PROPERTIES
        OUTPUT_NAME "westeros_plugin_rdkshell_shell"
        VERSION     1.0.0
        SOVERSION   1
        )

install(TARGETS RdkShellShell
        DESTINATION lib/plugins/westeros/
        )
