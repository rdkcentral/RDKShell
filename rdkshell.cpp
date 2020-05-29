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

#include <iostream>
#ifdef RDKSHELL_ENABLE_WEBSOCKET_IPC
#include "messageHandler.h"
#endif
#include "essosinstance.h"
#include "compositorcontroller.h"
#include <unistd.h>
#include <time.h>

#define RDKSHELL_FPS 30
int gCurrentFramerate = RDKSHELL_FPS;
bool gRdkShellIsRunning = false;
#ifdef RDKSHELL_ENABLE_WEBSOCKET_IPC
std::shared_ptr<RdkShell::MessageHandler> gMessageHandler;
bool gWebsocketIpcEnabled = false;
#endif

namespace RdkShell
{
    double milliseconds()
    {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ((double)(ts.tv_sec * 1000) + ((double)ts.tv_nsec/1000000));
    }

    double microseconds()
    {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ((double)(ts.tv_sec * 1000000) + ((double)ts.tv_nsec/1000));
    }

    void initialize()
    {
        std::cout << "initializing rdk shell\n";

        char const *s = getenv("RDKSHELL_FRAMERATE");
        if (s)
        {
            int fps = atoi(s);
            if (fps > 0)
            {
                gCurrentFramerate = fps;
            }
        }

        #ifdef RDKSHELL_ENABLE_WEBSOCKET_IPC
        char const* websocketIpcSetting = getenv("RDKSHELL_ENABLE_WS_IPC");
        if (websocketIpcSetting && (strcmp(websocketIpcSetting,"1") == 0))
        {
            gWebsocketIpcEnabled = true;
        }
        if (gWebsocketIpcEnabled)
        {
            gMessageHandler = std::make_shared<RdkShell::MessageHandler>(3000);
            gMessageHandler->start();
        }
        #endif
        RdkShell::EssosInstance::instance()->initialize(false);
    }

    void run()
    {
        gRdkShellIsRunning = true;
        while( gRdkShellIsRunning )
        {
            const double maxSleepTime = (1000 / gCurrentFramerate) * 1000;
            double startFrameTime = microseconds();
            RdkShell::CompositorController::draw();
            RdkShell::EssosInstance::instance()->update();
            #ifdef RDKSHELL_ENABLE_WEBSOCKET_IPC
            if (gWebsocketIpcEnabled)
            {
                gMessageHandler->poll();
            }
            #endif
            double frameTime = (int)microseconds() - (int)startFrameTime;
            int32_t sleepTimeInMs = gCurrentFramerate - frameTime;
            if (frameTime < maxSleepTime)
            {
                int sleepTime = (int)maxSleepTime-(int)frameTime;
                usleep(sleepTime);
            }
        }
    }

    void draw()
    {
        RdkShell::CompositorController::draw();
        RdkShell::EssosInstance::instance()->update();
    }
}
