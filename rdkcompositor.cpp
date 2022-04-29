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

#include "rdkcompositor.h"
#include "compositorcontroller.h"

#include <iostream>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "linuxkeys.h"
#include "rdkshell.h"
#include "permissions.h"
#include "framebuffer.h"
#include "framebufferrenderer.h"
#include "logger.h"

extern bool gForce720;

namespace RdkShell
{
    #define RDKSHELL_INITIAL_INPUT_LISTENER_TAG 1001

    void launchApplicationThreadCallback(RdkCompositor* compositor)
    {
        if (compositor != nullptr)
        {
            compositor->launchApplication();
        }
    }

    RdkCompositor::RdkCompositor() : mDisplayName(), mWstContext(NULL), 
        mWidth(1920), mHeight(1080), mPositionX(0), mPositionY(0), mMatrix(), mOpacity(1.0),
        mVisible(true), mAnimating(false), mHolePunch(true), mScaleX(1.0), mScaleY(1.0), mEnableKeyMetadata(false), mInputListenerTags(RDKSHELL_INITIAL_INPUT_LISTENER_TAG), mInputLock(), mInputListeners(),
        mApplicationName(), mApplicationThread(), mApplicationState(RdkShell::ApplicationState::Unknown),
        mApplicationPid(-1), mApplicationThreadStarted(false), mApplicationClosedByCompositor(false), mApplicationMutex(), mReceivedKeyPress(false),
        mVirtualDisplayEnabled(false), mVirtualWidth(0), mVirtualHeight(0), mSizeChangeRequestPresent(false), mSurfaceCount(0),
        mInputEventsEnabled(true)
    {
        if (gForce720)
        {
            RdkShell::Logger::log(LogLevel::Information,  "forcing 720 for rdkc");
            mWidth = 1280;
            mHeight = 720;
        }
        float* matrixPointer = mMatrix;
        float matrix[16] = 
        {
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 0.0, 0.0, 1.0
        };
        
        memcpy(matrixPointer, matrix, sizeof(matrix));
    }
    
    RdkCompositor::~RdkCompositor()
    {
        if ( mWstContext )
        {
            WstCompositorSetInvalidateCallback(mWstContext, NULL, NULL);
            WstCompositorSetClientStatusCallback(mWstContext, NULL, NULL);
            WstCompositorSetDispatchCallback( mWstContext, NULL, NULL);
            closeApplication();
            WstCompositorDestroy(mWstContext);
            //shutdownApplication();
        }
        mWstContext = NULL;

        mInputListeners.clear();
        mReceivedKeyPress = false;
    }

    void RdkCompositor::invalidate(WstCompositor */*context*/, void *userData)
    {
        RdkCompositor *rdkCompositor= (RdkCompositor*)userData;
        if (rdkCompositor != NULL)
        {
            rdkCompositor->onInvalidate();
        }
    }

    void RdkCompositor::clientStatus(WstCompositor */*context*/, int status, int pid, int detail, void *userData)
    {
        RdkCompositor *rdkCompositor= (RdkCompositor*)userData;
        if (rdkCompositor != NULL)
        {
            rdkCompositor->onClientStatus(status, pid, detail);
        }
    }

    void RdkCompositor::dispatch( WstCompositor *wctx, void *userData )
    {
         RdkCompositor *rdkCompositor= (RdkCompositor*)userData;
         if (rdkCompositor != NULL)
         {
             rdkCompositor->onSizeChangeComplete();
         }
    }

    void RdkCompositor::onInvalidate()
    {
        //todo
    }

    void RdkCompositor::onClientStatus(int status, int pid, int detail)
    {
        if (mApplicationPid < 0)
        {
            if ((status == WstClient_stoppedAbnormal) || (status == WstClient_stoppedNormal))
            {
                mApplicationPid = -1;
            }
            else
            {
                mApplicationPid = pid;
            }
        }
        bool eventFound = true;
        std::string eventName = "";

        switch ( status )
        {
             case WstClient_started:
                 RdkShell::Logger::log(LogLevel::Information,  "client started");
                 eventName = RDKSHELL_EVENT_APPLICATION_LAUNCHED;
                 break;
             case WstClient_stoppedNormal:
                 RdkShell::Logger::log(LogLevel::Information,  "client stopped normal");
                 eventName = RDKSHELL_EVENT_APPLICATION_TERMINATED;
                 break;
             case WstClient_stoppedAbnormal:
                 RdkShell::Logger::log(LogLevel::Information,  "client stopped abnormal");
                 eventName = RDKSHELL_EVENT_APPLICATION_TERMINATED;
                 break;
             case WstClient_connected:
                 RdkShell::Logger::log(LogLevel::Information,  "client connected");
                 eventName = RDKSHELL_EVENT_APPLICATION_CONNECTED;
                 break;
             case WstClient_disconnected:
                 RdkShell::Logger::log(LogLevel::Information,  "client disconnected");
                 eventName = RDKSHELL_EVENT_APPLICATION_DISCONNECTED;
                 break;
             case WstClient_firstFrame:
                 RdkShell::Logger::log(LogLevel::Information,  "client first frame received");
                 eventName = RDKSHELL_EVENT_APPLICATION_FIRST_FRAME;
                 break;
             default:
                 RdkShell::Logger::log(LogLevel::Information,  "unknown client status state");
                 eventFound = false;
                 break;
        }

        if (eventFound)
        {
            onEvent(eventName);
        }
    }

    void RdkCompositor::onSizeChangeComplete()
    {
        if (mSizeChangeRequestPresent)
        {		
            mSizeChangeRequestPresent = false;
            onEvent(RDKSHELL_EVENT_SIZE_CHANGE_COMPLETE);
        }
    }

    bool RdkCompositor::loadExtensions(WstCompositor *compositor, const std::string& clientName)
    {
        RdkShell::Logger::log(LogLevel::Information,  "loadExtensions clientName: %s", clientName.c_str());

        bool success = true;
        if (compositor)
        {
            const char* enableRdkShellExtendedInput = getenv("RDKSHELL_EXTENDED_INPUT_ENABLED");

            if (enableRdkShellExtendedInput)
            {
                std::string extensionInputPath = std::string(RDKSHELL_WESTEROS_PLUGIN_DIRECTORY) + "libwesteros_plugin_rdkshell_extended_input.so";
                RdkShell::Logger::log(LogLevel::Information,  "attempting to load extension: %s", extensionInputPath.c_str());
                if (!WstCompositorAddModule(compositor, extensionInputPath.c_str()))
                {
                    RdkShell::Logger::log(LogLevel::Information,  "Failed to load plugin: libwesteros_plugin_rdkshell_extended_input.so");
                    success = false;
                }
            }

            std::vector<std::string> extensions;
            getAllowedExtensions(clientName, extensions);

            RdkShell::Logger::log(LogLevel::Information,  "loadExtensions getAllowedExtensions found: %d extensions for client %s ", extensions.size(), clientName.c_str());

            for (int i = 0; i < extensions.size(); ++i)
            {
                const std::string extensionInputPath = RDKSHELL_WESTEROS_PLUGIN_DIRECTORY + extensions[i];
                RdkShell::Logger::log(LogLevel::Information,  "attempting to load extension: %s", extensionInputPath.c_str());
                if (!WstCompositorAddModule(compositor, extensionInputPath.c_str()))
                {
                    RdkShell::Logger::log(LogLevel::Information,  "Failed to load plugin: libwesteros_plugin_rdkshell_client_control.so");
                    success = false;
                }
            }
        }
        else
        {
            success = false;
        }

        return success;
    }

    void RdkCompositor::prepareHolePunchRects(std::vector<WstRect> rects, RdkShellRect& rect)
    {
        uint32_t x=20000, y=20000, w=0, h=0;
        for (int i=0; i<rects.size(); i++)
        {
            WstRect& wstRect = rects[i];
            x = (wstRect.x < x)?wstRect.x:x;
            y = (wstRect.y < y)?wstRect.y:y;
            int xBoundary = wstRect.x + wstRect.width;
            int yBoundary = wstRect.y + wstRect.height;
            int tempWidth = xBoundary-x;
            int tempHeight = yBoundary-y;
            w = (tempWidth>w)?tempWidth:w;
            h = (tempHeight>h)?tempHeight:h;
        }
        rect.x = x;
        rect.y = y;
        rect.width = w;
        rect.height = h;
        RdkShell::Logger::log(LogLevel::Debug,  "hole punch rectangle: x %d y %d w %d h %d", x, y, w, h);
    }

    void RdkCompositor::draw(bool &needsHolePunch, RdkShellRect& rect)
    {
        #ifndef RDKSHELL_ENABLE_HIDDEN_SUPPORT
        if (!mVisible)
        {
            return;
        }
        #endif //!RDKSHELL_ENABLE_HIDDEN_SUPPORT

        if (mVirtualDisplayEnabled)
        {
            drawFbo(needsHolePunch, rect);
        }
        else
        {
            drawDirect(needsHolePunch, rect);
        }
    }

    void RdkCompositor::drawFbo(bool &needsHolePunch, RdkShellRect& rect)
    {
        // create the FBO if it's not created yet or its size was changed
        if (!mFbo ||
            mFbo->width() != mVirtualWidth ||
            mFbo->height() != mVirtualHeight)
        {
            RdkShell::Logger::log(LogLevel::Information,  "creating FBO resolution: %d x %d", mVirtualWidth, mVirtualHeight);
            mFbo = std::make_shared<FrameBuffer>(mVirtualWidth, mVirtualHeight);
        }

        unsigned int outputWidth, outputHeight;
        WstCompositorGetOutputSize(mWstContext, &outputWidth, &outputHeight);
        if ((mVirtualWidth != (uint32_t)outputWidth) || (mVirtualHeight != (uint32_t)outputHeight))
        {
            WstCompositorSetOutputSize(mWstContext, mVirtualWidth, mVirtualHeight);
        }

        mFbo->bind();
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        glViewport(0, 0, mVirtualWidth, mVirtualHeight);

        int hints = WstHints_none;//WstHints_fboTarget;
        std::vector<WstRect> rects;
        float opacity = 1.f;
        float matrix[16] = {
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f
        };

        WstCompositorComposeEmbedded(mWstContext, 0, 0, mVirtualWidth, mVirtualHeight,
            matrix, opacity, hints, &needsHolePunch, rects);

        if (needsHolePunch)
        {
            prepareHolePunchRects(rects, rect);
        }

        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        mFbo->unbind();

        uint32_t screenWidth, screenHeight;
        CompositorController::getScreenResolution(screenWidth, screenHeight);

        FrameBufferRenderer::instance()->draw(mFbo, screenWidth, screenHeight, mMatrix,
            mPositionX, mPositionY, mWidth, mHeight);
    }

    void RdkCompositor::drawDirect(bool &needsHolePunch, RdkShellRect& rect)
    {
        int hints = WstHints_none;
        hints |= WstHints_applyTransform;
        if (mHolePunch)
        {
            hints |= WstHints_holePunch;
        }
        hints |= WstHints_noRotation;
        if (mAnimating)
        {
            hints |= WstHints_animating;
        }
        #ifdef RDKSHELL_ENABLE_FORCE_ANIMATE
        hints |= WstHints_animating;
        #endif //RDKSHELL_ENABLE_FORCE_ANIMATE
        #ifdef RDKSHELL_ENABLE_HIDDEN_SUPPORT
        if (!mVisible)
        {
            hints |= WstHints_hidden;
        }
        #endif //RDKSHELL_ENABLE_HIDDEN_SUPPORT
        std::vector<WstRect> rects;

        unsigned int width, height;
        WstCompositorGetOutputSize(mWstContext, &width, &height);
        if ( (mWidth != (uint32_t)width) || (mHeight != (uint32_t)height) )
        {
            WstCompositorSetOutputSize(mWstContext, mWidth, mHeight);
        }

        WstCompositorComposeEmbedded( mWstContext, 0, 0, mWidth, mHeight,
            mMatrix, mOpacity, hints, &needsHolePunch, rects );
        if (needsHolePunch)
        {
            prepareHolePunchRects(rects, rect);
        }
    }

    void RdkCompositor::processKeyEvent(bool keyPressed, uint32_t keycode, uint32_t flags, uint64_t metadata)
    {
        if (!mInputEventsEnabled)
        {
            RdkShell::Logger::log(LogLevel::Information, "processKeyEvent input event blocked disp:%s, keyCode: %d",
                mDisplayName, keycode);
            return;
        }

        uint32_t modifiers = 0;

        if ( flags & RDKSHELL_FLAGS_SHIFT )
        {
            modifiers |= WstKeyboard_shift;
        }
        if ( flags & RDKSHELL_FLAGS_CONTROL )
        {
            modifiers |= WstKeyboard_ctrl;
        }
        if ( flags & RDKSHELL_FLAGS_ALT )
        {
            modifiers |= WstKeyboard_alt;
        }

        int32_t waylandKeyCode = (int32_t)keyCodeToWayland(keycode);

        WstCompositorKeyEvent( mWstContext, waylandKeyCode, keyPressed ? WstKeyboard_keyState_depressed : WstKeyboard_keyState_released, (int32_t)modifiers );
        if (mEnableKeyMetadata)
        {
            RdkShell::InputEvent inputEvent(metadata, RdkShell::milliseconds(), RdkShell::InputEvent::KeyEvent);
            inputEvent.details.key.code = waylandKeyCode;
            inputEvent.details.key.state = keyPressed ? RdkShell::InputEvent::Details::Key::Pressed : RdkShell::InputEvent::Details::Key::Released;
            broadcastInputEvent(inputEvent);
        }
    }


    void RdkCompositor::onKeyPress(uint32_t keycode, uint32_t flags, uint64_t metadata)
    {
        processKeyEvent(true, keycode, flags, metadata);
        mReceivedKeyPress = true;
    }

    void RdkCompositor::onKeyRelease(uint32_t keycode, uint32_t flags, uint64_t metadata)
    {
        if (mReceivedKeyPress)
        {
            processKeyEvent(false, keycode, flags, metadata);
        }
        mReceivedKeyPress = false;
    }

    void RdkCompositor::onPointerMotion(uint32_t x, uint32_t y)
    {
        WstCompositorPointerMoveEvent(mWstContext, x, y);
    }

    void RdkCompositor::onPointerButtonPress(uint32_t keyCode, uint32_t x, uint32_t y)
    {
        WstCompositorPointerButtonEvent(mWstContext, keyCode, WstKeyboard_keyState_depressed);
    }

    void RdkCompositor::onPointerButtonRelease(uint32_t keyCode, uint32_t x, uint32_t y)
    {
        WstCompositorPointerButtonEvent(mWstContext, keyCode, WstKeyboard_keyState_released);
    }

    void RdkCompositor::setPosition(int32_t x, int32_t y)
    {
        mPositionX = x;
        mPositionY = y;
        mMatrix[12] = x;
        mMatrix[13] = y;
    }

    void RdkCompositor::position(int32_t &x, int32_t &y)
    {
        x = mPositionX;
        y = mPositionY;
    }

    void RdkCompositor::setOpacity(double opacity)
    {
        mOpacity = opacity;
    }

    void RdkCompositor::scale(double &scaleX, double &scaleY)
    {
        scaleX = mScaleX;
        scaleY = mScaleY;
    }

    void RdkCompositor::setScale(double scaleX, double scaleY)
    {
        if (scaleX >= 0)
        {
            mScaleX = scaleX;
        }
        if (scaleY >= 0)
        {
            mScaleY = scaleY;
        }

        mMatrix[0] = 1 * mScaleX;
        mMatrix[5] = 1 * mScaleY;
    }

    void RdkCompositor::setSize(uint32_t width, uint32_t height)
    {
        if (gForce720)
        {
            width = 1280;
            height = 720;
        }
        if ( (mWstContext != NULL) && !mVirtualDisplayEnabled && ((mWidth != width) || (mHeight != height)) )
        {
            mSizeChangeRequestPresent = true;
            WstCompositorSetOutputSize(mWstContext, width, height);
        }
        mWidth = width;
        mHeight = height;
    }

    void RdkCompositor::size(uint32_t &width, uint32_t &height)
    {
        width = mWidth;
        height = mHeight;
    }

    void RdkCompositor::opacity(double& opacity)
    {
        opacity = mOpacity;
    }

    void RdkCompositor::setVisible(bool visible)
    {
        mVisible = visible;
    }
    
    void RdkCompositor::visible(bool &visible)
    {
        visible = mVisible;
    }

    void RdkCompositor::setAnimating(bool animating)
    {
        mAnimating = animating;
    }

    void RdkCompositor::setHolePunch(bool holePunchEnabled)
    {
        mHolePunch = holePunchEnabled;
    }

    void RdkCompositor::holePunch(bool &holePunchEnabled)
    {
        holePunchEnabled = mHolePunch;
    }

    void RdkCompositor::keyMetadataEnabled(bool &enabled)
    {
        enabled = mEnableKeyMetadata;
    }

    void RdkCompositor::setKeyMetadataEnabled(bool enable)
    {
        mEnableKeyMetadata = enable;
    }

    int RdkCompositor::registerInputEventListener(std::function<void(const RdkShell::InputEvent&)> listener)
    {
        std::lock_guard<std::mutex> locker(mInputLock);
        const int tag = mInputListenerTags++;
        mInputListeners.emplace(tag, std::move(listener));
        return tag;
    }

    void RdkCompositor::unregisterInputEventListener(int tag)
    {
        std::lock_guard<std::mutex> locker(mInputLock);
        mInputListeners.erase(tag);
    }

    void RdkCompositor::broadcastInputEvent(const RdkShell::InputEvent &inputEvent)
    {
        RdkShell::Logger::log(LogLevel::Information,  "sending input metadata for device: %d", inputEvent.deviceId);
        std::lock_guard<std::mutex> locker(mInputLock);
        for (const auto &listener : mInputListeners)
        {
            if (listener.second)
                listener.second(inputEvent);
        }
    }

    void RdkCompositor::displayName(std::string& name) const
    {
        name = mDisplayName;
    }

    void RdkCompositor::launchApplicationInBackground()
    {
        mApplicationThreadStarted = true;
        mApplicationThread = std::thread{launchApplicationThreadCallback, this};
    }

    void RdkCompositor::launchApplication()
    {
        std::string applicationName;
        {
          std::lock_guard<std::recursive_mutex> lock{mApplicationMutex};
          mApplicationState = RdkShell::ApplicationState::Running;
          applicationName = mApplicationName;
        }
        if (!WstCompositorLaunchClient(mWstContext, applicationName.c_str()))
        {
            RdkShell::Logger::log(LogLevel::Information,  "RdkCompositor failed to launch %s", applicationName.c_str());
            const char *detail = WstCompositorGetLastErrorDetail( mWstContext );
            RdkShell::Logger::log(LogLevel::Information,  "westeros error: %s", detail);
        }
        RdkShell::Logger::log(LogLevel::Information,  "application close: %s", applicationName.c_str());
        {
            std::lock_guard<std::recursive_mutex> lock{mApplicationMutex};
            mApplicationState = RdkShell::ApplicationState::Running;
        }
    }

    void RdkCompositor::closeApplication()
    {
        {
            std::lock_guard<std::recursive_mutex> lock{mApplicationMutex};
            if (mApplicationPid > 0 &&
                mApplicationState != RdkShell::ApplicationState::Stopped &&
                mApplicationState != RdkShell::ApplicationState::Unknown)
            {
                RdkShell::Logger::log(LogLevel::Information,  "about to terminate process id %d", mApplicationPid);
                kill( mApplicationPid, SIGKILL);
                RdkShell::Logger::log(LogLevel::Information,  "process with id %d has been terminated", mApplicationPid);
                mApplicationPid = 0;
                mApplicationState = RdkShell::ApplicationState::Stopped;
            }
        }
        if (mApplicationThreadStarted)
        {
            mApplicationThread.join();
        }
    }

    bool RdkCompositor::resumeApplication()
    {
        std::lock_guard<std::recursive_mutex> lock{mApplicationMutex};
        if (mApplicationState == RdkShell::ApplicationState::Suspended)
        {
            mApplicationState = RdkShell::ApplicationState::Running;
            onEvent(RDKSHELL_EVENT_APPLICATION_RESUMED);
            return true;
        }
        return false;
    }

    bool RdkCompositor::suspendApplication()
    {
        std::lock_guard<std::recursive_mutex> lock{mApplicationMutex};
        if (mApplicationState == RdkShell::ApplicationState::Running)
        {
            mApplicationState = RdkShell::ApplicationState::Suspended;
            onEvent(RDKSHELL_EVENT_APPLICATION_SUSPENDED);
            return true;
        }
        return false;
    }

    void RdkCompositor::shutdownApplication()
    {
        std::lock_guard<std::recursive_mutex> lock{mApplicationMutex};
        if (mApplicationClosedByCompositor && (mApplicationPid > 0) && (0 == kill(mApplicationPid, 0)))
        {
            RdkShell::Logger::log(LogLevel::Information,  "sending SIGKILL to application with pid %d", mApplicationPid);
            kill(mApplicationPid, SIGKILL);
            mApplicationClosedByCompositor = false;
        }
        mApplicationPid= -1;
    }

    void RdkCompositor::setApplication(const std::string& application)
    {
        mApplicationName = application;
    }

    bool RdkCompositor::isKeyPressed()
    {
        return mReceivedKeyPress;
    }

    void RdkCompositor::getVirtualResolution(uint32_t &virtualWidth, uint32_t &virtualHeight)
    {
        virtualWidth = mVirtualWidth;
        virtualHeight = mVirtualHeight;
    }

    void RdkCompositor::setVirtualResolution(uint32_t virtualWidth, uint32_t virtualHeight)
    {
        mVirtualWidth = (virtualWidth > 0) ? virtualWidth : mWidth;
        mVirtualHeight = (virtualHeight > 0) ? virtualHeight : mHeight;
    }

    void RdkCompositor::enableVirtualDisplay(bool enable)
    {
        mVirtualDisplayEnabled = enable;   
    }

    bool RdkCompositor::getVirtualDisplayEnabled()
    {
        return mVirtualDisplayEnabled;
    }

    void RdkCompositor::updateSurfaceCount (bool status)
    {
        if (status == true)
        {
            mSurfaceCount++;
        }
        else if ((status == false) && (mSurfaceCount > 0))
        {
            mSurfaceCount--;
        } 
    }

    uint32_t RdkCompositor::getSurfaceCount (void)
    {
        return mSurfaceCount;
    }

    void RdkCompositor::enableInputEvents(bool enable)
    {
        RdkShell::Logger::log(LogLevel::Information, "enableInputEvents disp:%s, oldVal: %d, newVal: %d",
            mDisplayName, mInputEventsEnabled, enable);
        mInputEventsEnabled = enable;
    }

    bool RdkCompositor::getInputEventsEnabled() const
    {
        return mInputEventsEnabled;
     }
    

    void sendApplicationEvent(std::shared_ptr<RdkShellEventListener>& listener, const std::string& eventName, const std::string& client)
    {
         if (eventName.compare(RDKSHELL_EVENT_APPLICATION_LAUNCHED) == 0)
         {
                 listener->onApplicationLaunched(client);
         }
         else if(eventName.compare(RDKSHELL_EVENT_APPLICATION_TERMINATED) == 0)
         {
                 listener->onApplicationTerminated(client);
         }
         else if(eventName.compare(RDKSHELL_EVENT_APPLICATION_CONNECTED) == 0)
         {
                 listener->onApplicationConnected(client);
         }
         else if(eventName.compare(RDKSHELL_EVENT_APPLICATION_DISCONNECTED) == 0)
         {
                 listener->onApplicationDisconnected(client);
         }
         else if(eventName.compare(RDKSHELL_EVENT_APPLICATION_FIRST_FRAME) == 0)
         {
                 listener->onApplicationFirstFrame(client);
         }
         else if(eventName.compare(RDKSHELL_EVENT_APPLICATION_SUSPENDED) == 0)
         {
                 listener->onApplicationSuspended(client);
         }
         else if(eventName.compare(RDKSHELL_EVENT_APPLICATION_RESUMED) == 0)
         {
                 listener->onApplicationResumed(client);
         }
         else if(eventName.compare(RDKSHELL_EVENT_SIZE_CHANGE_COMPLETE) == 0)
         {
                 listener->onSizeChangeComplete(client);
         }
    }

    void RdkCompositor::onEvent(std::string const& eventName)
    {
        for (auto& listener : mEventListeners)
        {
            sendApplicationEvent(listener, eventName, mName);
        }

        if (!CompositorController::isSurfaceModeEnabled()) return;

        if (eventName == RDKSHELL_EVENT_APPLICATION_CONNECTED)
        {
            mSurfaceCount++;
        }
        else if (eventName == RDKSHELL_EVENT_APPLICATION_DISCONNECTED && mSurfaceCount > 0)
        {
            mSurfaceCount--;
        }

        if (getSurfaceCount() == 0 && mAutoDestroy) // FIXME
        {
            CompositorController::kill(""); // FIXME
        }
    }

    void RdkCompositor::addEventListener(std::shared_ptr<RdkShellEventListener> listener)
    {
        mEventListeners.push_back(listener);
    }

    void RdkCompositor::removeEventListener(std::shared_ptr<RdkShellEventListener> listener)
    {
        auto it = std::find(mEventListeners.begin(), mEventListeners.end(), listener);
        if (it != mEventListeners.end())
        {
            mEventListeners.erase(it);
        }
    }

    void RdkCompositor::addKeyListener(uint32_t keyCode, uint32_t flags, bool activate, bool propagate)
    {
        KeyListenerInfo info;
        info.keyCode = keyCode;
        info.flags = flags;
        info.activate = activate;
        info.propagate = propagate;

        bool found = false;
        auto& keyListeners = mKeyListeners[keyCode];

        for (auto& listener : keyListeners)
        {
            if (listener.flags == flags)
            {
                listener = info;
                found = true;
                break;
            }
        }

        if (!found)
        {
            keyListeners.push_back(info);
        }
    }

    void RdkCompositor::removeKeyListener(uint32_t keyCode, uint32_t flags)
    {
        auto it = mKeyListeners.find(keyCode);
        if (it == mKeyListeners.end()) return;

        auto& listeners = it->second;
        auto listenerIt = std::find_if(listeners.begin(), listeners.end(), [flags](auto& listener) { return listener.flags == flags; });

        if (listenerIt == listeners.end()) return;

        if (isKeyPressed())
        {
            // TODO: add compositor to gPendingKeyUpListeners
        }

        listeners.erase(listenerIt);

        if (listeners.size() == 0)
        {
            mKeyListeners.erase(keyCode);
        }
    }

    void RdkCompositor::evaluateKeyListeners(uint32_t keycode, uint32_t flags, bool& found, bool& activate, bool& propagate)
    {
        found = false;
        
        auto it = mKeyListeners.find(keycode);
        if (it != mKeyListeners.end())
        {
            for (auto& listener : it->second)
            {
                if (listener.flags == flags)
                {
                    found = true;
                    activate = listener.activate;
                    propagate = listener.propagate;
                    break;
                }
            }
        }

        if (!found)
        {
            auto wildcardIt = mKeyListeners.find(RDKSHELL_ANY_KEY);
            if (wildcardIt != mKeyListeners.end())
            {
                auto& listener = wildcardIt->second[0];
                found = true;
                activate = listener.activate;
                propagate = listener.propagate;
            }
        }
    }

    void RdkCompositor::removeAllKeyListeners()
    {
        mKeyListeners.clear();
    }

    void RdkCompositor::removeAllEventListeners()
    {
        mEventListeners.clear();
    }

    void RdkCompositor::setMimeType(std::string const& mimetype)
    {
        mMimeType = mimetype;
    }

    void RdkCompositor::mimeType(std::string& mimetype)
    {
        mimetype = mMimeType;
    }

    void RdkCompositor::setAutoDestroy(bool autoDestroy)
    {
        mAutoDestroy = autoDestroy;
    }
}
