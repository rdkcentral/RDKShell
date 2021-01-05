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

#ifndef RDKSHELL_COMPOSITOR_CONTROLLER_H
#define RDKSHELL_COMPOSITOR_CONTROLLER_H

#include "rdkshelldata.h"

#include "rdkshellevents.h"
#include "rdkcompositor.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>

namespace RdkShell
{
    class CompositorController
    {
        public:
            static void initialize();
            static bool moveToFront(const std::string& client);
            static bool moveToBack(const std::string& client);
            static bool moveBehind(const std::string& client, const std::string& target);
            static bool setFocus(const std::string& client);
            static bool kill(const std::string& client);
            static bool addKeyIntercept(const std::string& client, const uint32_t& keyCode, const uint32_t& flags);
            static bool removeKeyIntercept(const std::string& client, const uint32_t& keyCode, const uint32_t& flags);
            static bool addKeyListener(const std::string& client, const uint32_t& keyCode, const uint32_t& flags, std::map<std::string, RdkShellData> &listenerProperties);
            static bool addNativeKeyListener(const std::string& client, const uint32_t& keyCode, const uint32_t& flags, std::map<std::string, RdkShellData> &listenerProperties);
            static bool removeKeyListener(const std::string& client, const uint32_t& keyCode, const uint32_t& flags);
            static bool removeNativeKeyListener(const std::string& client, const uint32_t& keyCode, const uint32_t& flags);
            static bool addKeyMetadataListener(const std::string& client);
            static bool removeKeyMetadataListener(const std::string& client);
            static bool injectKey(const uint32_t& keyCode, const uint32_t& flags);
            static bool generateKey(const std::string& client, const uint32_t& keyCode, const uint32_t& flags);
            static bool getScreenResolution(uint32_t &width, uint32_t &height);
            static bool setScreenResolution(const uint32_t width, const uint32_t height);
            static bool getClients(std::vector<std::string>& clients);
            static bool getZOrder(std::vector<std::string>&clients);
            static bool getBounds(const std::string& client, uint32_t &x, uint32_t &y, uint32_t &width, uint32_t &height);
            static bool setBounds(const std::string& client, const uint32_t x, const uint32_t y, const uint32_t width, const uint32_t height);
            static bool getVisibility(const std::string& client, bool& visible);
            static bool setVisibility(const std::string& client, const bool visible);
            static bool getOpacity(const std::string& client, unsigned int& opacity);
            static bool setOpacity(const std::string& client, const unsigned int opacity);
            static bool getScale(const std::string& client, double &scaleX, double &scaleY);
            static bool setScale(const std::string& client, double scaleX, double scaleY);
            static bool getHolePunch(const std::string& client, bool& holePunch);
            static bool setHolePunch(const std::string& client, const bool holePunch);
            static bool scaleToFit(const std::string& client, const int32_t x, const int32_t y, const uint32_t width, const uint32_t height);
            static void onKeyPress(uint32_t keycode, uint32_t flags, uint64_t metadata);
            static void onKeyRelease(uint32_t keycode, uint32_t flags, uint64_t metadata);
            static bool createDisplay(const std::string& client, const std::string& displayName, uint32_t displayWidth=0, uint32_t displayHeight=0);
            static bool addAnimation(const std::string& client, double duration, std::map<std::string, RdkShellData> &animationProperties);
            static bool removeAnimation(const std::string& client);
            static bool addListener(const std::string& client, std::shared_ptr<RdkShellEventListener> listener);
            static bool removeListener(const std::string& client, std::shared_ptr<RdkShellEventListener> listener);
            static bool onEvent(RdkCompositor* eventCompositor, const std::string& eventName);
            static void enableInactivityReporting(const bool enable);
            static void setInactivityInterval(const double minutes);
            static double getInactivityTimeInMinutes();
            static void setEventListener(std::shared_ptr<RdkShellEventListener> listener);
            static std::shared_ptr<RdkCompositor> getCompositor(const std::string& displayName);
            static bool launchApplication(const std::string& client, const std::string& uri, const std::string& mimeType);
            static bool suspendApplication(const std::string& client);
            static bool resumeApplication(const std::string& client);
            static bool closeApplication(const std::string& client);
            static bool getMimeType(const std::string& client, std::string& mimeType);
            static bool setMimeType(const std::string& client, const std::string& mimeType);
            static bool hideSplashScreen();
            static bool showSplashScreen(uint32_t displayTimeInSeconds);
            static bool hideWatermark();
            static bool showWatermark();
            static bool draw();
            static bool update();
            static bool setLogLevel(const std::string level);
            static bool getLogLevel(std::string& level);
            static bool sendEvent(const std::string& eventName, std::vector<std::map<std::string, RdkShellData>>& data);
    };
}

#endif //RDKSHELL_COMPOSITOR_CONTROLLER_H
