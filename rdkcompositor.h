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
#include <functional>
#include <unordered_map>
#include "westeros-compositor.h"
#include "inputevent.h"
#include "application.h"
#include "rdkshellrect.h"
#include "rdkshellevents.h"
#include "node.h"

#define RDKSHELL_ANY_KEY 65536

namespace RdkShell
{

    class FrameBuffer;

    struct KeyListenerInfo
    {
        KeyListenerInfo() : keyCode(-1), flags(0), activate(false), propagate(true) {}
        uint32_t keyCode;
        uint32_t flags;
        bool activate;
        bool propagate;
    };

    class RdkCompositor : public Node
    {
        public:

            RdkCompositor();
            virtual ~RdkCompositor();
            virtual bool createDisplay(const std::string& displayName, const std::string& clientName,
                uint32_t width, uint32_t height, bool virtualDisplayEnabled, uint32_t virtualWidth, uint32_t virtualHeight) = 0;
            void draw(bool &needsHolePunch, RdkShellRect& rect) override;
            void onKeyPress(uint32_t keycode, uint32_t flags, uint64_t metadata);
            void onKeyRelease(uint32_t keycode, uint32_t flags, uint64_t metadata);
            void onPointerMotion(uint32_t x, uint32_t y);
            void onPointerButtonPress(uint32_t keyCode, uint32_t x, uint32_t y);
            void onPointerButtonRelease(uint32_t keyCode, uint32_t x, uint32_t y);
            void setPosition(int32_t x, int32_t y) override;
            void position(int32_t &x, int32_t &y) override;
            void setSize(uint32_t width, uint32_t height) override;
            void size(uint32_t &width, uint32_t &height) override;
            void setOpacity(double opacity) override;
            void scale(double &scaleX, double &scaleY) override;
            void setScale(double scaleX, double scaleY) override;
            void opacity(double& opacity) override;
            void setVisible(bool visible) override;
            void visible(bool &visible) override;
            void setAnimating(bool animating) override;
            void setHolePunch(bool holePunchEnabled) override;
            void holePunch(bool &holePunchEnabled) override;
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
            void getVirtualResolution(uint32_t &virtualWidth, uint32_t &virtualHeight);
            void setVirtualResolution(uint32_t virtualWidth, uint32_t virtualHeight);
            void enableVirtualDisplay(bool enable);
            bool getVirtualDisplayEnabled();
            void updateSurfaceCount(bool status);
            uint32_t getSurfaceCount(void);
            void enableInputEvents(bool enable);
            bool getInputEventsEnabled() const;
            
            void addKeyListener(uint32_t keyCode, uint32_t flags, bool activate, bool propagate);
            void removeKeyListener(uint32_t keyCode, uint32_t flags);
            void removeAllKeyListeners();
            
            void addEventListener(std::shared_ptr<RdkShellEventListener> listener);
            void removeEventListener(std::shared_ptr<RdkShellEventListener> listener);
            void removeAllEventListeners();

            void setMimeType(std::string const& mimetype);
            void mimeType(std::string& mimetype);

            void setAutoDestroy(bool autoDestroy);
            
            void evaluateKeyListeners(uint32_t keycode, uint32_t flags, bool& foundlistener, bool& activate, bool& propagate);

        private:
            void prepareHolePunchRects(std::vector<WstRect> wstrects, RdkShellRect& rect);
            uint32_t mSurfaceCount;

            void onEvent(std::string const& eventName);
            std::map<uint32_t, std::vector<KeyListenerInfo>> mKeyListeners;
            std::vector<std::shared_ptr<RdkShellEventListener>> mEventListeners;
            std::string mMimeType;
            bool mAutoDestroy;
            
        protected:
            static void invalidate(WstCompositor *context, void *userData);
            static void clientStatus(WstCompositor *context, int status, int pid, int detail, void *userData);
            static void dispatch( WstCompositor *wctx, void *userData );
            void onInvalidate();
            void onClientStatus(int status, int pid, int detail);
            void onSizeChangeComplete();
            void processKeyEvent(bool keyPressed, uint32_t keycode, uint32_t flags, uint64_t metadata);
            void broadcastInputEvent(const RdkShell::InputEvent &inputEvent);
            void launchApplicationInBackground();
            void shutdownApplication();
            static bool loadExtensions(WstCompositor *compositor, const std::string& clientName);
            void drawDirect(bool &needsHolePunch, RdkShellRect& rect);
            void drawFbo(bool &needsHolePunch, RdkShellRect& rect);

            std::shared_ptr<RdkCompositor> compositor() override { return shared_from(this); } 
            
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
            bool mVirtualDisplayEnabled;
            uint32_t mVirtualWidth;
            uint32_t mVirtualHeight;
            std::shared_ptr<FrameBuffer> mFbo;
            bool mSizeChangeRequestPresent;
            bool mInputEventsEnabled;
    };
}

#endif //RDKSHELL_RDK_COMPOSITOR_H
