/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2020 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#pragma once

#include <typeindex>
#include <string>

namespace RdkShell
{
    class RdkShellEventListener
    {
        public:
          virtual void onApplicationLaunched(const std::string& client) {}
          virtual void onApplicationConnected(const std::string& client) {}
          virtual void onApplicationDisconnected(const std::string& client)  {}
          virtual void onApplicationTerminated(const std::string& client) {}
          virtual void onApplicationFirstFrame(const std::string& client) {}
          virtual void onUserInactive(const double minutes) {}
    };

    const std::string RDKSHELL_EVENT_APPLICATION_LAUNCHED = "onApplicationLaunched";
    const std::string RDKSHELL_EVENT_APPLICATION_CONNECTED = "onApplicationConnected";
    const std::string RDKSHELL_EVENT_APPLICATION_DISCONNECTED = "onApplicationDisconnected";
    const std::string RDKSHELL_EVENT_APPLICATION_TERMINATED = "onApplicationTerminated";
    const std::string RDKSHELL_EVENT_APPLICATION_FIRST_FRAME = "onApplicationFirstFrame";
    const std::string RDKSHELL_EVENT_USER_INACTIVE = "onUserInactive";
}
