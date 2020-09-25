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
#include <map>
#include <vector>
#include "rdkshelldata.h"

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
          virtual void onApplicationSuspended(const std::string& client) {}
          virtual void onApplicationResumed(const std::string& client) {}
          virtual void onUserInactive(const double minutes) {}
          virtual void onDeviceLowRamWarning(const int32_t freeKb) {}
          virtual void onDeviceCriticallyLowRamWarning(const int32_t freeKb) {}
          virtual void onDeviceLowRamWarningCleared(const int32_t freeKb) {}
          virtual void onDeviceCriticallyLowRamWarningCleared(const int32_t freeKb) {}
          virtual void onAnimation(std::vector<std::map<std::string, RdkShellData>>& animationData) {}
    };

    const std::string RDKSHELL_EVENT_APPLICATION_LAUNCHED = "onApplicationLaunched";
    const std::string RDKSHELL_EVENT_APPLICATION_CONNECTED = "onApplicationConnected";
    const std::string RDKSHELL_EVENT_APPLICATION_DISCONNECTED = "onApplicationDisconnected";
    const std::string RDKSHELL_EVENT_APPLICATION_TERMINATED = "onApplicationTerminated";
    const std::string RDKSHELL_EVENT_APPLICATION_FIRST_FRAME = "onApplicationFirstFrame";
    const std::string RDKSHELL_EVENT_APPLICATION_SUSPENDED = "onApplicationSuspended";
    const std::string RDKSHELL_EVENT_APPLICATION_RESUMED = "onApplicationResumed";
    const std::string RDKSHELL_EVENT_USER_INACTIVE = "onUserInactive";
    const std::string RDKSHELL_EVENT_DEVICE_LOW_RAM_WARNING = "onDeviceLowRamWarning";
    const std::string RDKSHELL_EVENT_DEVICE_CRITICALLY_LOW_RAM_WARNING = "onDeviceCriticallyLowRamWarning";
    const std::string RDKSHELL_EVENT_DEVICE_LOW_RAM_WARNING_CLEARED = "onDeviceLowRamWarningCleared";
    const std::string RDKSHELL_EVENT_DEVICE_CRITICALLY_LOW_RAM_WARNING_CLEARED = "onDeviceCriticallyLowRamWarningCleared";
    const std::string RDKSHELL_EVENT_ANIMATION = "onAnimation";
}
