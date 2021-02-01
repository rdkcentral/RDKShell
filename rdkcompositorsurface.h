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

#ifndef RDKSHELL_RDK_COMPOSITOR_SURFACE_H
#define RDKSHELL_RDK_COMPOSITOR_SURFACE_H

#include <string>
#include <thread>
#include <mutex>

#include "rdkcompositor.h"
#include <mutex>
#include <functional>
#include <unordered_map>
#include "westeros-compositor.h"
#include "inputevent.h"
#include "application.h"

namespace RdkShell
{
    class RdkCompositorSurface:public RdkCompositor
    {
        public:
            static void createMainCompositor(const std::string& displayName, uint32_t width, uint32_t height);
            bool createDisplay(const std::string& displayName, const std::string& clientName,
                uint32_t width, uint32_t height, bool virtualDisplayEnabled, uint32_t virtualWidth, uint32_t virtualHeight);
        private:
            static WstCompositor *mMainWstContext;
            static uint32_t mMainCompositorWidth, mMainCompositorHeight;
            static std::string mMainCompositorDisplayName;
    };
}

#endif //RDKSHELL_RDK_COMPOSITOR_SURFACE_H
