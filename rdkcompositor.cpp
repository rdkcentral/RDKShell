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
        mApplicationPid(-1), mApplicationThreadStarted(false), mApplicationClosedByCompositor(false), mApplicationMutex(), mReceivedKeyPress(false)
    {
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
                 std::cout << "client started\n";
                 eventName = RDKSHELL_EVENT_APPLICATION_LAUNCHED;
                 break;
             case WstClient_stoppedNormal:
                 std::cout << "client stopped normal\n";
                 eventName = RDKSHELL_EVENT_APPLICATION_TERMINATED;
                 break;
             case WstClient_stoppedAbnormal:
                 std::cout << "client stopped abnormal\n";
                 eventName = RDKSHELL_EVENT_APPLICATION_TERMINATED;
                 break;
             case WstClient_connected:
                 std::cout << "client connected\n";
                 eventName = RDKSHELL_EVENT_APPLICATION_CONNECTED;
                 break;
             case WstClient_disconnected:
                 std::cout << "client disconnected\n";
                 eventName = RDKSHELL_EVENT_APPLICATION_DISCONNECTED;
                 break;
             case WstClient_firstFrame:
                 std::cout << "client first frame received\n";
                 eventName = RDKSHELL_EVENT_APPLICATION_FIRST_FRAME;
                 break;
             default:
                 std::cout << "unknown client status state\n";
                 eventFound = false;
                 break;
        }

        if (eventFound)
        {
            CompositorController::onEvent(this, eventName);
        }
    }

    bool RdkCompositor::createDisplay(const std::string& displayName, uint32_t width, uint32_t height)
    {
        if (width > 0 && height > 0)
        {
            mWidth = width;
            mHeight = height;
        }
        mWstContext = WstCompositorCreate();

        bool error = false;

        if (mWstContext)
        {
            const char* enableRdkShellExtendedInput = getenv("RDKSHELL_EXTENDED_INPUT_ENABLED");

            if (enableRdkShellExtendedInput)
            {
                std::string extensionInputPath = std::string(RDKSHELL_WESTEROS_PLUGIN_DIRECTORY) + "libwesteros_plugin_rdkshell_extended_input.so";
                std::cout << "attempting to load extension: " << extensionInputPath << std::endl << std::flush;
                if (!WstCompositorAddModule(mWstContext, extensionInputPath.c_str()))
                {
                    std::cout << "Faild to load plugin: 'libwesteros_plugin_rdkshell_extended_input.so'" << std::endl;
                    error = true;
                }
            }

            if (!WstCompositorSetIsEmbedded(mWstContext, true))
            {
                error = true;
            }

            if (!error && !WstCompositorSetOutputSize(mWstContext, mWidth, mHeight))
            {
                error = true;
            }

            if (!error && !WstCompositorSetInvalidateCallback( mWstContext, invalidate, this))
            {
                error = true;
            }

            if (!error && !WstCompositorSetClientStatusCallback( mWstContext, clientStatus, this))
            {
                error = true;
            }

            if (!error)
            {
                if (!displayName.empty())
                {
                    if (!WstCompositorSetDisplayName( mWstContext, displayName.c_str()))
                    {
                        error = true;
                    }
                }
                if (mDisplayName.empty())
                {
                    mDisplayName = WstCompositorGetDisplayName(mWstContext);
                }
                std::cout << "The display name is: " << mDisplayName << std::endl;
                
                if (!error && !WstCompositorStart(mWstContext))
                {
                    error= true;
                }

                if (!mApplicationName.empty())
                {
                    std::cout << "RDKShell is launching " << mApplicationName << std::endl;
                    launchApplicationInBackground();
                }
            }
        }

        if (error)
        {
            const char *detail= WstCompositorGetLastErrorDetail( mWstContext );
            std::cout << "error setting up the compositor: " << detail << std::endl;
            return false;
        }
        return true;
    }

    void RdkCompositor::draw()
    {
        #ifndef RDKSHELL_ENABLE_HIDDEN_SUPPORT
        if (!mVisible)
        {
            return;
        }
        #endif //!RDKSHELL_ENABLE_HIDDEN_SUPPORT
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
        bool needsHolePunch = false;
        std::vector<WstRect> rects;

        unsigned int width, height;
        WstCompositorGetOutputSize(mWstContext, &width, &height);
        if ( (mWidth != (uint32_t)width) || (mHeight != (uint32_t)height) )
        {
            WstCompositorSetOutputSize(mWstContext, mWidth, mHeight);
        }

        WstCompositorComposeEmbedded( mWstContext, 0, 0, mWidth, mHeight,
            mMatrix, mOpacity, hints, &needsHolePunch, rects );
    }

    void RdkCompositor::processKeyEvent(bool keyPressed, uint32_t keycode, uint32_t flags, uint64_t metadata)
    {
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
        if ( (mWstContext != NULL) && ((mWidth != width) || (mHeight != height)) )
        {
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
        std::cout << "sending input metadata for device: " << inputEvent.deviceId << std::endl;
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
            std::cout << "RdkCompositor failed to launch " << applicationName << std::endl;
            const char *detail = WstCompositorGetLastErrorDetail( mWstContext );
            std::cout << "westeros error: " << detail << std::endl;
        }
        std::cout << "application close: " << applicationName << std::endl << std::flush;
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
                std::cout << "about to terminate process id " << mApplicationPid << std::endl;
                kill( mApplicationPid, SIGKILL);
                std::cout << "process with id " << mApplicationPid << " has been terminated" << std::endl;
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
            CompositorController::onEvent(this, RDKSHELL_EVENT_APPLICATION_RESUMED);
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
            CompositorController::onEvent(this, RDKSHELL_EVENT_APPLICATION_SUSPENDED);
            return true;
        }
        return false;
    }

    void RdkCompositor::shutdownApplication()
    {
        std::lock_guard<std::recursive_mutex> lock{mApplicationMutex};
        if (mApplicationClosedByCompositor && (mApplicationPid > 0) && (0 == kill(mApplicationPid, 0)))
        {
            std::cout << "sending SIGKILL to application with pid " <<  mApplicationPid << std::endl;
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
}
