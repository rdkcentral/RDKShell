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
#include "linuxkeys.h"

namespace RdkShell
{
    RdkCompositor::RdkCompositor() : mDisplayName(), mWstContext(NULL), 
        mWidth(1280), mHeight(720), mPositionX(0), mPositionY(0), mMatrix(), mOpacity(1.0),
        mVisible(true), mAnimating(false), mScaleX(1.0), mScaleY(1.0)
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
            WstCompositorDestroy(mWstContext);
        }
        mWstContext = NULL;
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
        if (!mVisible)
        {
            return;
        }
        int hints = WstHints_none;
        hints |= WstHints_applyTransform;
        hints |= WstHints_holePunch;
        hints |= WstHints_noRotation;
        if (mAnimating)
        {
            hints |= WstHints_animating;
        }
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

    void RdkCompositor::onKeyPress(uint32_t keycode, uint32_t flags)
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

        WstCompositorKeyEvent( mWstContext, waylandKeyCode, WstKeyboard_keyState_depressed, (int32_t)modifiers );
    }

    void RdkCompositor::onKeyRelease(uint32_t keycode, uint32_t flags)
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

        WstCompositorKeyEvent( mWstContext, waylandKeyCode, WstKeyboard_keyState_released, (int32_t)modifiers );
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
}
