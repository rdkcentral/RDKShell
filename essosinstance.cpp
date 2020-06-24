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

#include "essosinstance.h"

#include "linuxkeys.h"
#include "compositorcontroller.h"

#include <iostream>


namespace RdkShell
{
    EssosInstance* EssosInstance::mInstance = NULL;
}

static void essosTerminated( void * )
{
}

static EssTerminateListener essosTerminateListener=
{
    essosTerminated
};

static bool rightShiftPressed = false;
static bool leftShiftPressed = false;
static bool rightAltPressed = false;
static bool leftAltPressed = false;
static bool rightCtrlPressed = false;
static bool leftCtrlPressed = false;

static void essosKeyPressed( void *userData, unsigned int key )
{
    switch( key )
    {
        case WAYLAND_KEY_RIGHTSHIFT:
            rightShiftPressed = true;
        break;
        case WAYLAND_KEY_LEFTSHIFT:
            leftShiftPressed = true;
        break;
        case WAYLAND_KEY_RIGHTCTRL:
            rightCtrlPressed = true;
        break;
        case WAYLAND_KEY_LEFTCTRL:
            leftCtrlPressed = true;
        break;
        case WAYLAND_KEY_RIGHTALT:
            rightAltPressed = true;
        break;
        case WAYLAND_KEY_LEFTALT:
            leftAltPressed = true;
        break;
        default:
            break;
    }
    unsigned long flags = 0;
    flags |= (rightShiftPressed || leftShiftPressed) ? RDKSHELL_FLAGS_SHIFT:0;
    flags |= (rightCtrlPressed || leftCtrlPressed) ? RDKSHELL_FLAGS_CONTROL:0;
    flags |= (rightAltPressed || leftAltPressed) ? RDKSHELL_FLAGS_ALT:0;

    uint32_t mappedKeyCode = key, mappedFlags = 0;
    bool ret = keyCodeFromWayland(key, flags, mappedKeyCode, mappedFlags);

    // todo - read in the metadata
    uint64_t keyMetadata = 0;

    RdkShell::EssosInstance::instance()->onKeyPress(mappedKeyCode, mappedFlags, keyMetadata);
}

static void essosKeyReleased( void *userData, unsigned int key )
{
    switch( key )
    {
        case WAYLAND_KEY_RIGHTSHIFT:
            rightShiftPressed = false;
        break;
        case WAYLAND_KEY_LEFTSHIFT:
            leftShiftPressed = false;
        break;
        case WAYLAND_KEY_RIGHTCTRL:
            rightCtrlPressed = false;
        break;
        case WAYLAND_KEY_LEFTCTRL:
            leftCtrlPressed = false;
        break;
        case WAYLAND_KEY_RIGHTALT:
            rightAltPressed = false;
        break;
        case WAYLAND_KEY_LEFTALT:
            leftAltPressed = false;
        break;
        default:
            break;
    }
    unsigned long flags = 0;
    flags |= (rightShiftPressed || leftShiftPressed) ? RDKSHELL_FLAGS_SHIFT:0;
    flags |= (rightCtrlPressed || leftCtrlPressed) ? RDKSHELL_FLAGS_CONTROL:0;
    flags |= (rightAltPressed || leftAltPressed) ? RDKSHELL_FLAGS_ALT:0;

    uint32_t mappedKeyCode = key, mappedFlags = 0;
    bool ret = keyCodeFromWayland(key, flags, mappedKeyCode, mappedFlags);

    // todo - read in the metadata
    uint64_t keyMetadata = 0;

    RdkShell::EssosInstance::instance()->onKeyRelease(mappedKeyCode, mappedFlags, keyMetadata);
}

static EssKeyListener essosKeyListener=
{
    essosKeyPressed,
    essosKeyReleased
};

static void essosDisplaySize( void *userData, int width, int height )
{
    RdkShell::EssosInstance::instance()->onDisplaySizeChanged((uint32_t)width, (uint32_t)height);
}

static EssSettingsListener essosSettingsListener =
{
    essosDisplaySize
};

namespace RdkShell
{
    EssosInstance::EssosInstance() : mEssosContext(NULL), mUseWayland(false),
           mWidth(0), mHeight(0), mOverrideResolution(false), mKeyInitialDelay(500), mKeyRepeatInterval(250)
    {
    }
    
    EssosInstance::~EssosInstance()
    {
        if (mEssosContext)
        {
            EssContextDestroy(mEssosContext);
        }
        mEssosContext = NULL;
    }

    EssosInstance* EssosInstance::instance()
    {
        if (mInstance == NULL)
        {
            mInstance = new EssosInstance();
        }
        return mInstance;
    }

    void EssosInstance::initialize(bool useWayland)
    {
        mUseWayland = useWayland;
        mEssosContext = EssContextCreate();
        int32_t width = 0;
        int32_t height = 0;

        bool essosError = false;
        if (mEssosContext)
        {
            if ( !EssContextSetUseWayland( mEssosContext, mUseWayland ) )
            {
                essosError = true;
            }
            if ( !EssContextSetTerminateListener( mEssosContext, 0, &essosTerminateListener ) )
            {
                essosError = true;
            }

            if ( !EssContextSetKeyListener( mEssosContext, 0, &essosKeyListener ) )
            {
                essosError = true;
            }
            if ( !EssContextSetKeyRepeatInitialDelay(mEssosContext, (int)mKeyInitialDelay))
            {
                essosError = true;
            }
            if ( !EssContextSetKeyRepeatPeriod(mEssosContext, (int)mKeyRepeatInterval))
            {
                essosError = true;
            }
            if (!mOverrideResolution)
            {
                if (!EssContextSetSettingsListener(mEssosContext, 0, &essosSettingsListener))
                {
                    essosError = true;
                }
            }
            if ( !essosError )
            {
                if ( !EssContextStart( mEssosContext ) )
                {
                    essosError = true;
                }
                else if ( !EssContextGetDisplaySize( mEssosContext, &width, &height ) )
                {
                    essosError = true;
                }
            }
            if (mOverrideResolution)
            {
                std::cout << "forcing resolution to: " <<  mWidth << " x " << mHeight << std::endl;
                if ( !EssContextSetInitialWindowSize( mEssosContext, mWidth, mHeight) )
                {
                    essosError = true;
                }
            }
            else if (!essosError)
            {
                mWidth = (uint32_t)width;
                mHeight = (uint32_t)height;
            }
            
            if ( essosError )
            {
                const char *errorDetail = EssContextGetLastErrorDetail(mEssosContext);
                std::cout << "Essos error during initialization: " << errorDetail << std::endl;
            }
        }
    }

    void EssosInstance::initialize(bool useWayland, uint32_t width, uint32_t height)
    {
        mWidth = width;
        mHeight = height;
        mOverrideResolution = true;
        initialize(useWayland);
    }

    void EssosInstance::configureKeyInput(uint32_t initialDelay, uint32_t repeatInterval)
    {
        mKeyInitialDelay = initialDelay;
        mKeyRepeatInterval = repeatInterval;
    }

    void EssosInstance::onKeyPress(uint32_t keyCode, unsigned long flags, uint64_t metadata)
    {
        CompositorController::onKeyPress(keyCode, flags, metadata);
    }

    void EssosInstance::onKeyRelease(uint32_t keyCode, unsigned long flags, uint64_t metadata)
    {
        CompositorController::onKeyRelease(keyCode, flags, metadata);
    }

    void EssosInstance::onDisplaySizeChanged(uint32_t width, uint32_t height)
    {
        if (mInstance)
        {
            EssContextResizeWindow( mEssosContext, (int)width, (int)height );
        }
        mWidth = width;
        mHeight = height;
    }

    void EssosInstance::resolution(uint32_t &width, uint32_t &height)
    {
        width = mWidth;
        height = mHeight;
    }

    void EssosInstance::setResolution(uint32_t width, uint32_t height)
    {
        onDisplaySizeChanged(width, height);
    }

    void EssosInstance::update()
    {
        if (mEssosContext)
        {
            EssContextUpdateDisplay(mEssosContext);
            EssContextRunEventLoopOnce(mEssosContext);
        }
    }
}
