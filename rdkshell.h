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

#ifndef RDKSHELL_H
#define RDKSHELL_H

#include "rdkshelldata.h"
#include <map>
#include <string>

namespace RdkShell
{
    void initialize();
    void run();
    void update();
    void draw();
    void deinitialize();
    double seconds();
    double milliseconds();
    double microseconds();
    bool systemRam(uint32_t& freeKb, uint32_t& totalKb, uint32_t& availableKb, uint32_t& usedSwapKb);
    bool systemRam(uint32_t& freeKb, uint32_t& totalKb, uint32_t& usedSwapKb);
    void setMemoryMonitor(const bool enable, const double interval);
    void setMemoryMonitor(std::map<std::string, RdkShellData> &configuration);
}

#endif //RDKSHELL_H
