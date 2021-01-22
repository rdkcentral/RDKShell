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

add_library(RdkShellClientControlWesterosPlugin 
        SHARED
        source/rdkshellclientcontrolmodule.cpp
        protocol/rdkshell_client_control_protocol.c
        )

target_link_libraries(RdkShellClientControlWesterosPlugin rdkshell_shared)

target_include_directories(RdkShellClientControlWesterosPlugin
        PRIVATE
        protocol
        $<TARGET_PROPERTY:rdkshell_shared,INTERFACE_INCLUDE_DIRECTORIES>
        )

set_target_properties(RdkShellClientControlWesterosPlugin
        PROPERTIES
        OUTPUT_NAME "westeros_plugin_rdkshell_client_control"
        VERSION     1.0.0
        SOVERSION   1
        )

install(TARGETS RdkShellClientControlWesterosPlugin
        DESTINATION lib/plugins/westeros/
        )
