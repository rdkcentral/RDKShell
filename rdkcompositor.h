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

#ifndef RDKSHELL_RDK_COMPOSITOR_H
#define RDKSHELL_RDK_COMPOSITOR_H

#include <string>
#include <thread>
#include <mutex>

#include <mutex>
#include <functional>
#include <unordered_map>
#include "westeros-compositor.h"
#include "inputevent.h"
#include "application.h"

namespace RdkShell
{
    class RdkCompositor
    {
        public:

            RdkCompositor();
            virtual ~RdkCompositor();
            virtual bool createDisplay(const std::string& displayName, const std::string& clientName, uint32_t width, uint32_t height) = 0;
            void draw();
            void onKeyPress(uint32_t keycode, uint32_t flags, uint64_t metadata);
            void onKeyRelease(uint32_t keycode, uint32_t flags, uint64_t metadata);
            void setPosition(int32_t x, int32_t y);
            void position(int32_t &x, int32_t &y);
            void setSize(uint32_t width, uint32_t height);
            void size(uint32_t &width, uint32_t &height);
            void setOpacity(double opacity);
            void scale(double &scaleX, double &scaleY);
            void setScale(double scaleX, double scaleY);
            void opacity(double& opacity);
            void setVisible(bool visible);
            void visible(bool &visible);
            void setAnimating(bool animating);
            void setHolePunch(bool holePunchEnabled);
            void holePunch(bool &holePunchEnabled);
            void keyMetadataEnabled(bool &enabled);
            void setKeyMetadataEnabled(bool enable);
            int registerInputEventListener(std::function<void(const RdkShell::InputEvent&)> listener);
            void unregisterInputEventListener(int tag);
            void displayName(std::string& name) const;
            void closeApplication();
            void launchApplication();
            bool resumeApplication();
            bool suspendApplication();
            void setApplication(const std::string& application);
            bool isKeyPressed();

        protected:
            static void invalidate(WstCompositor *context, void *userData);
            static void clientStatus(WstCompositor *context, int status, int pid, int detail, void *userData);
            void onInvalidate();
            void onClientStatus(int status, int pid, int detail);
            void processKeyEvent(bool keyPressed, uint32_t keycode, uint32_t flags, uint64_t metadata);
            void broadcastInputEvent(const RdkShell::InputEvent &inputEvent);
            void launchApplicationInBackground();
            void shutdownApplication();
            static bool loadExtensions(WstCompositor *compositor, const std::string& clientName);
            
            std::string mDisplayName;
            WstCompositor *mWstContext;
            uint32_t mWidth;
            uint32_t mHeight;
            int32_t mPositionX;
            int32_t mPositionY;
            float mMatrix[16];
            double mOpacity;
            bool mVisible;
            bool mAnimating;
            bool mHolePunch;
            double mScaleX;
            double mScaleY;
            bool mEnableKeyMetadata;
            int mInputListenerTags;
            std::mutex mInputLock;
            std::unordered_map<int, std::function<void(const RdkShell::InputEvent&)>> mInputListeners;
            std::string mApplicationName;
            std::thread mApplicationThread;
            RdkShell::ApplicationState mApplicationState;
            int32_t mApplicationPid;
            bool mApplicationThreadStarted;
            bool mApplicationClosedByCompositor;
            std::recursive_mutex mApplicationMutex;
            bool mReceivedKeyPress;
    };
}

#endif //RDKSHELL_RDK_COMPOSITOR_H
