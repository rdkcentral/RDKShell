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
#include <GLES2/gl2.h>
#ifdef RDKSHELL_ENABLE_IPC
#include "servermessagehandler.h"
#endif

#ifdef RDKSHELL_ENABLE_WEBSOCKET_IPC
#include "messageHandler.h"
#endif
#include "essosinstance.h"
#include "compositorcontroller.h"
#include "linuxkeys.h"
#include "linuxinput.h"
#include "animation.h"
#include "logger.h"
#include "rdkshell.h"
#include <unistd.h>
#include <time.h>
#include <GLES2/gl2.h>
#include <sys/sysinfo.h>

#define RDKSHELL_FPS 40

#define RDKSHELL_RAM_MONITOR_INTERVAL_SECONDS 5
#define RDKSHELL_DEFAULT_LOW_MEMORY_THRESHOLD_MB 100
#define RDKSHELL_DEFAULT_CRITICALLY_LOW_MEMORY_THRESHOLD_MB 20

int gCurrentFramerate = RDKSHELL_FPS;
bool gRdkShellIsRunning = false;

bool gEnableRamMonitor = true;
double gRamMonitorIntervalInSeconds = RDKSHELL_RAM_MONITOR_INTERVAL_SECONDS;
double gLowRamMemoryThresholdInMb =  RDKSHELL_DEFAULT_LOW_MEMORY_THRESHOLD_MB;
double gCriticallyLowRamMemoryThresholdInMb = RDKSHELL_DEFAULT_CRITICALLY_LOW_MEMORY_THRESHOLD_MB;

bool gLowRamMemoryNotificationSent = false;
bool gCriticallyLowRamMemoryNotificationSent = false;
double gNextRamMonitorTime = 0.0;

#ifdef RDKSHELL_ENABLE_IPC
std::shared_ptr<RdkShell::ServerMessageHandler> gServerMessageHandler;
bool gIpcEnabled = false;
#endif

#ifdef RDKSHELL_ENABLE_WEBSOCKET_IPC
std::shared_ptr<RdkShell::MessageHandler> gMessageHandler;
bool gWebsocketIpcEnabled = false;
#endif

namespace RdkShell
{

    double seconds()
    {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts.tv_sec + ((double)ts.tv_nsec/1000000000);
    }

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

    bool systemRam(uint32_t &freeKb, uint32_t & totalKb,  uint32_t& usedSwapKb)
    {
        struct sysinfo systemInformation;
        int ret = sysinfo(&systemInformation);

        if (0 != ret)
        {
          Logger::log(Debug, "failed to get memory details");
          return false;
        }
        totalKb = systemInformation.totalram/(1024);
        freeKb = systemInformation.freeram/(1024);
        usedSwapKb = (systemInformation.totalswap - systemInformation.freeswap)/1024;
        return true;
    }

    void setMemoryMonitor(const bool enable, const double interval)
    {
        gEnableRamMonitor = enable;
        gRamMonitorIntervalInSeconds = interval;
    }

    void setMemoryMonitor(std::map<std::string, RdkShellData> &configuration)
    {
        for ( const auto &monitorConfiguration : configuration )
        {
            if (monitorConfiguration.first == "enable")
            {
                gEnableRamMonitor = monitorConfiguration.second.toBoolean();
            }
            else if (monitorConfiguration.first == "interval")
            {
                gRamMonitorIntervalInSeconds = monitorConfiguration.second.toDouble();
            }
            else if (monitorConfiguration.first == "lowRam")
            {
                gLowRamMemoryThresholdInMb = monitorConfiguration.second.toDouble();
            }
            else if (monitorConfiguration.first == "criticallyLowRam")
            {
                gCriticallyLowRamMemoryThresholdInMb = monitorConfiguration.second.toDouble();
            }
        }
        if (gCriticallyLowRamMemoryThresholdInMb  > gLowRamMemoryThresholdInMb)
        {
            Logger::log(Warn, "criticial low ram threshold configuration is lower than low ram threshold");
            gCriticallyLowRamMemoryThresholdInMb = gLowRamMemoryThresholdInMb;
        }
    }

    void checkSystemMemory()
    {
        uint32_t freeKb=0, usedKb=0, totalKb=0;
        bool ret = systemRam(freeKb, usedKb, totalKb);

        if (false == ret)
        {
            return;
        }

        float freeMb = freeKb/1024;
        std::vector<std::map<std::string, RdkShellData>> eventData(1);
        eventData[0] = std::map<std::string, RdkShellData>();
        eventData[0]["freeKb"] = freeKb;
        if (freeMb < gLowRamMemoryThresholdInMb)
        {
            if (!gLowRamMemoryNotificationSent)
            {
                CompositorController::sendEvent(RDKSHELL_EVENT_DEVICE_LOW_RAM_WARNING, eventData);
                gLowRamMemoryNotificationSent = true;
            }
            if ((!gCriticallyLowRamMemoryNotificationSent) && (freeMb < gCriticallyLowRamMemoryThresholdInMb))
            {
                  CompositorController::sendEvent(RDKSHELL_EVENT_DEVICE_CRITICALLY_LOW_RAM_WARNING, eventData);
                  gCriticallyLowRamMemoryNotificationSent = true;
            }
            else if ((gCriticallyLowRamMemoryNotificationSent) && (freeMb >= gCriticallyLowRamMemoryThresholdInMb))
            {
                CompositorController::sendEvent(RDKSHELL_EVENT_DEVICE_CRITICALLY_LOW_RAM_WARNING_CLEARED, eventData);
                gCriticallyLowRamMemoryNotificationSent = false;
            }
        }
        else
        {
            if (gCriticallyLowRamMemoryNotificationSent)
            {
                CompositorController::sendEvent(RDKSHELL_EVENT_DEVICE_CRITICALLY_LOW_RAM_WARNING_CLEARED, eventData);
                gCriticallyLowRamMemoryNotificationSent = false;
            }
            if (gLowRamMemoryNotificationSent)
            {
                CompositorController::sendEvent(RDKSHELL_EVENT_DEVICE_LOW_RAM_WARNING_CLEARED, eventData);
                gLowRamMemoryNotificationSent = false;
            }
        }
    }

    void initialize()
    {
        std::cout << "initializing rdk shell\n";

        mapNativeKeyCodes();
        readInputDevicesConfiguration();

        char const *loglevel = getenv("RDKSHELL_LOG_LEVEL");
        if (loglevel)
        {
            Logger::setLogLevel(loglevel);
        }

        char const *s = getenv("RDKSHELL_FRAMERATE");
        if (s)
        {
            int fps = atoi(s);
            if (fps > 0)
            {
                gCurrentFramerate = fps;
            }
        }

        char const *lowRamMemoryThresholdInMb = getenv("RDKSHELL_LOW_MEMORY_THRESHOLD");
        if (lowRamMemoryThresholdInMb)
        {
            double lowRamMemoryThresholdInMbValue = std::stod(lowRamMemoryThresholdInMb);
            if (lowRamMemoryThresholdInMbValue > 0)
            {
                gLowRamMemoryThresholdInMb = lowRamMemoryThresholdInMbValue;
            }
        }

        char const *criticalLowRamMemoryThresholdInMb = getenv("RDKSHELL_CRITICALLY_LOW_MEMORY_THRESHOLD");
        if (criticalLowRamMemoryThresholdInMb)
        {
            double criticalLowRamMemoryThresholdInMbValue = std::stod(criticalLowRamMemoryThresholdInMb);
            if (criticalLowRamMemoryThresholdInMbValue > 0)
            {
                if (criticalLowRamMemoryThresholdInMbValue  <= gLowRamMemoryThresholdInMb)
                {
                    gCriticallyLowRamMemoryThresholdInMb = criticalLowRamMemoryThresholdInMbValue;
                }
                else
                {
                    Logger::log(Warn, "criticial low ram threshold is lower than low ram threshold");
                    gCriticallyLowRamMemoryThresholdInMb = gLowRamMemoryThresholdInMb;
                }
            }
        }

        uint32_t initialKeyDelay = 500;
        char const *keyDelay = getenv("RDKSHELL_KEY_INITIAL_DELAY");
        if (keyDelay)
        {
            int value = atoi(keyDelay);
            if (value > 0)
            {
                initialKeyDelay = value;
            }
        }

        uint32_t repeatKeyInterval = 100;
        char const *repeatInterval = getenv("RDKSHELL_KEY_REPEAT_INTERVAL");
        if (repeatInterval)
        {
            int value = atoi(repeatInterval);
            if (value > 0)
            {
                repeatKeyInterval = value;
            }
        }

        RdkShell::EssosInstance::instance()->configureKeyInput(initialKeyDelay, repeatKeyInterval);

        #ifdef RDKSHELL_ENABLE_IPC
        char const* ipcSetting = getenv("RDKSHELL_ENABLE_IPC");
        if (ipcSetting && (strcmp(ipcSetting,"1") == 0))
        {
            gIpcEnabled = true;
        }
        if (gIpcEnabled)
        {
            gServerMessageHandler = std::make_shared<RdkShell::ServerMessageHandler>();
            gServerMessageHandler->start();
        }
        #endif

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

        #ifdef RDKSHELL_ENABLE_FORCE_1080
        std::cout << "!!!!! forcing 1080 start!\n";
        RdkShell::EssosInstance::instance()->initialize(false, 1920, 1080);
        #else
        RdkShell::EssosInstance::instance()->initialize(false);
        #endif //RDKSHELL_ENABLE_FORCE_1080
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        gNextRamMonitorTime = seconds() + gRamMonitorIntervalInSeconds;
    }

    void run()
    {
        gRdkShellIsRunning = true;
        while( gRdkShellIsRunning )
        {
            update();
            uint32_t width = 0;
            uint32_t height = 0;
            RdkShell::EssosInstance::instance()->resolution(width, height);
            glViewport( 0, 0, width, height );
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);

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
        uint32_t width = 0;
        uint32_t height = 0;
        RdkShell::EssosInstance::instance()->resolution(width, height);
        glViewport( 0, 0, width, height );
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        RdkShell::CompositorController::draw();
        RdkShell::EssosInstance::instance()->update();
    }

    void update()
    {
        if (gEnableRamMonitor)
        {
            double currentTime = RdkShell::seconds();
            if (currentTime > gNextRamMonitorTime)
            {
                checkSystemMemory();
                gNextRamMonitorTime = currentTime + gRamMonitorIntervalInSeconds;
            }
        }
        #ifdef RDKSHELL_ENABLE_IPC
        if (gIpcEnabled)
        {
            gServerMessageHandler->process();
        }
        #endif
        RdkShell::CompositorController::update();
    }
}
