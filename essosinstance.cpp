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
#include "linuxinput.h"
#include "inputdevicetypes.h"
#include "logger.h"

#include <iostream>

#ifdef ENABLE_ERM
#include <map>
#include <essos-resmgr.h>

    EssRMgr* gEssRMgr;
    std::map<std::string, bool> gAppsAVBlacklistStatus;
#endif
#ifdef RDKSHELL_ENABLE_KEY_METADATA
EssInputDeviceMetadata gInputDeviceMetadata = {};
#endif //RDKSHELL_ENABLE_KEY_METADATA

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

extern bool gForce720;

static uint8_t modeSettingForDev(uint8_t devType, uint8_t mode)
{
    switch (devType)
    {
        case RdkShell::DeviceType::GenericLinuxInputDev:
            // doesn't have trackpad or slider
            return (RdkShell::TrackPadDisabled | RdkShell::SliderDisabled);

        case RdkShell::DeviceType::Generic_IR:
            // IR device so obviously doesn't have slider or trackpad
            return (RdkShell::TrackPadDisabled | RdkShell::SliderDisabled);
        
        case RdkShell::DeviceType::VNC_Client:
        case RdkShell::DeviceType::FrontPanel:
            return 0xff;
    }

    return mode;
}

#ifdef RDKSHELL_ENABLE_KEY_METADATA
static uint32_t processInputMetadata(EssInputDeviceMetadata* metadata)
{
    uint8_t deviceType = static_cast<uint8_t>(RdkShell::DeviceType::GenericLinuxInputDev);
    uint8_t deviceMode = 0x0;
    uint16_t deviceId = minor(metadata->deviceNumber);

    RdkShell::inputDeviceTypeAndMode(metadata->id.vendor, metadata->id.product, metadata->devicePhysicalAddress, deviceType, deviceMode);
    deviceMode = modeSettingForDev(deviceType, deviceMode);

#ifdef RDKSHELL_ENABLE_KEY_METADATA_EXTENDED_SUPPORT_FOR_IR
    if (deviceType == static_cast<uint8_t>(RdkShell::DeviceType::Generic_IR))
    {
        // additional processing for IR
        RdkShell::irDeviceType(metadata->filterCode, deviceType);
    }
#endif

    // set the initial device info
    uint32_t deviceInfo = static_cast<uint32_t>(deviceType) << 24 |
                          static_cast<uint32_t>(deviceMode) << 16 |
                          static_cast<uint32_t>(deviceId);
    return deviceInfo;
}
#endif //RDKSHELL_ENABLE_KEY_METADATA

static void processKeyEvent(bool pressEvent, unsigned int key, void *metadata)
{
    switch( key )
    {
        case WAYLAND_KEY_RIGHTSHIFT:
            rightShiftPressed = pressEvent ? true : false;
        break;
        case WAYLAND_KEY_LEFTSHIFT:
            leftShiftPressed = pressEvent ? true : false;
        break;
        case WAYLAND_KEY_RIGHTCTRL:
            rightCtrlPressed = pressEvent ? true : false;
        break;
        case WAYLAND_KEY_LEFTCTRL:
            leftCtrlPressed = pressEvent ? true : false;
        break;
        case WAYLAND_KEY_RIGHTALT:
            rightAltPressed = pressEvent ? true : false;
        break;
        case WAYLAND_KEY_LEFTALT:
            leftAltPressed = pressEvent ? true : false;
        break;
        default:
            break;
    }
    unsigned long flags = 0;
    flags |= (rightShiftPressed || leftShiftPressed) ? RDKSHELL_FLAGS_SHIFT:0;
    flags |= (rightCtrlPressed || leftCtrlPressed) ? RDKSHELL_FLAGS_CONTROL:0;
    flags |= (rightAltPressed || leftAltPressed) ? RDKSHELL_FLAGS_ALT:0;

    uint32_t deviceInfo = 0;

    if (metadata != nullptr)
    {
#ifdef RDKSHELL_ENABLE_KEY_METADATA
        EssInputDeviceMetadata *essosMetadata = (EssInputDeviceMetadata*)(metadata);
        deviceInfo = processInputMetadata(essosMetadata);
#endif //RDKSHELL_ENABLE_KEY_METADATA
    }

    uint32_t mappedKeyCode = key, mappedFlags = 0;
    bool ret = keyCodeFromWayland(key, flags, mappedKeyCode, mappedFlags);

    if (pressEvent)
    {
        RdkShell::EssosInstance::instance()->onKeyPress(mappedKeyCode, mappedFlags, deviceInfo);
    }
    else
    {
        RdkShell::EssosInstance::instance()->onKeyRelease(mappedKeyCode, mappedFlags, deviceInfo);
    }
}

#ifdef RDKSHELL_ENABLE_KEY_METADATA
static void essosKeyAndMetadataPressed( void *userData, unsigned int key, EssInputDeviceMetadata *metadata )
{
    processKeyEvent(true, key, metadata);
}

static void essosKeyAndMetadataReleased( void *userData, unsigned int key, EssInputDeviceMetadata *metadata )
{
    processKeyEvent(false, key, metadata);
}

static EssKeyAndMetadataListener essosKeyAndMetadataListener=
{
    essosKeyAndMetadataPressed,
    essosKeyAndMetadataReleased
};
#endif //RDKSHELL_ENABLE_KEY_METADATA

static void essosKeyPressed( void *userData, unsigned int key )
{
    processKeyEvent(true, key, nullptr);
}

static void essosKeyReleased( void *userData, unsigned int key )
{
    processKeyEvent(false, key, nullptr);
}

static void essosKeyRepeat( void *userData, unsigned int key )
{
}

static EssKeyListener essosKeyListener=
{
    essosKeyPressed,
    essosKeyReleased
};

static void essosPointerMotion(void *userData, int x, int y)
{
    RdkShell::EssosInstance::instance()->onPointerMotion(x, y);
}

static void essosPointerButtonPressed(void *userData, int buttonId, int x, int y)
{
    RdkShell::EssosInstance::instance()->onPointerButtonPress(buttonId, x, y);
}

static void essosPointerButtonReleased(void *userData, int buttonId, int x, int y)
{
    RdkShell::EssosInstance::instance()->onPointerButtonRelease(buttonId, x, y);
}

static EssPointerListener pointerListener =
{
   essosPointerMotion,
   essosPointerButtonPressed,
   essosPointerButtonReleased
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
           mWidth(0), mHeight(0), mOverrideResolution(false), mKeyInitialDelay(500), mKeyRepeatInterval(250), mKeyInputsIgnored(false)
    {
        #ifdef RDKSHELL_ENABLE_KEYREPEATS
        mKeyRepeatsEnabled = true;
        #else
        mKeyRepeatsEnabled = false;
        essosKeyListener.keyRepeat = essosKeyRepeat;
        #endif
    }
    
    EssosInstance::~EssosInstance()
    {
        if (mEssosContext)
        {
            EssContextDestroy(mEssosContext);
        }
        mEssosContext = NULL;

#ifdef RDKSHELL_ENABLE_KEY_METADATA
        if (gInputDeviceMetadata.devicePhysicalAddress != 0)
        {
            free((char*)gInputDeviceMetadata.devicePhysicalAddress);
            gInputDeviceMetadata.devicePhysicalAddress = 0;
        }
#endif //RDKSHELL_ENABLE_KEY_METADATA
#ifdef ENABLE_ERM
            if (nullptr != gEssRMgr)
            {
                EssRMgrDestroy(gEssRMgr );
            }
#endif //ENABLE_ERM
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
#ifdef RDKSHELL_ENABLE_KEY_METADATA
            if ( !EssContextSetKeyAndMetadataListener( mEssosContext, 0, &essosKeyAndMetadataListener, &gInputDeviceMetadata ) )
            {
                essosError = true;
            }
#else
            if ( !EssContextSetKeyListener( mEssosContext, 0, &essosKeyListener ) )
            {
                essosError = true;
            }
#endif //RDKSHELL_ENABLE_KEY_METADATA
            if (!EssContextSetPointerListener(mEssosContext, 0, &pointerListener))
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
                RdkShell::Logger::log(LogLevel::Information,  "forcing resolution to: %d x %d" , mWidth ,mHeight);
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
                RdkShell::Logger::log(LogLevel::Information,  "Essos error during initialization: %s", errorDetail);
            }
        }
#ifdef ENABLE_ERM
        gEssRMgr = EssRMgrCreate();
        RdkShell::Logger::log(LogLevel::Information,  "EssRMgrCreate %s",(gEssRMgr != nullptr)?"succeeded":"failed");
#else
        RdkShell::Logger::log(LogLevel::Error,  "ENABLE_ERM not defined");
#endif
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
        RdkShell::Logger::log(LogLevel::Information,  "key initial delay: %d repeat interval: %d", mKeyInitialDelay ,mKeyRepeatInterval);
    }

    void EssosInstance::onKeyPress(uint32_t keyCode, unsigned long flags, uint64_t metadata)
    {
        if (mKeyInputsIgnored)
        {
            RdkShell::Logger::log(LogLevel::Information,  "key inputs ignored for press keycode: %d ", keyCode);
            return;
        }		
        CompositorController::onKeyPress(keyCode, flags, metadata);
    }

    void EssosInstance::onKeyRelease(uint32_t keyCode, unsigned long flags, uint64_t metadata)
    {
        if (mKeyInputsIgnored)
        {
            RdkShell::Logger::log(LogLevel::Information,  "key inputs ignored for release keycode: %d ", keyCode);
            return;
        }
        CompositorController::onKeyRelease(keyCode, flags, metadata);
    }

    void EssosInstance::onPointerMotion(uint32_t x, uint32_t y)
    {
        if (mKeyInputsIgnored)
            return;
        CompositorController::onPointerMotion(x, y);
    }

    void EssosInstance::onPointerButtonPress(uint32_t keyCode, uint32_t x, uint32_t y)
    {
        if (mKeyInputsIgnored)
        {
            RdkShell::Logger::log(LogLevel::Information,  "key inputs ignored for pointer button press keycode: %d ", keyCode);
	        return;
        }
        CompositorController::onPointerButtonPress(keyCode, x, y);
    }

    void EssosInstance::onPointerButtonRelease(uint32_t keyCode, uint32_t x, uint32_t y)
    {
        if (mKeyInputsIgnored)
        {
            RdkShell::Logger::log(LogLevel::Information,  "key inputs ignored for pointer button release keycode: %d ", keyCode);
	        return;
        }
        CompositorController::onPointerButtonRelease(keyCode, x, y);
    }

    void EssosInstance::onDisplaySizeChanged(uint32_t width, uint32_t height)
    {
#ifdef RDKSHELL_ENABLE_FORCE_1080
        if (gForce720)
        {
            width = 1280;
            height = 720;
        }
        else
        {
            width = 1920;
            height = 1080;
        }
#endif //RDKSHELL_ENABLE_FORCE_1080
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

    void EssosInstance::setKeyRepeats(bool enable)
    {
        // enabling key repeats will enable keydown events coming from rdkshell on keyhold till keyup is performed
        mKeyRepeatsEnabled = enable;
        if (mKeyRepeatsEnabled)
        {
            essosKeyListener.keyRepeat = nullptr;
        }
        else
        {
            essosKeyListener.keyRepeat = essosKeyRepeat; 
        }
    }

    void EssosInstance::keyRepeats(bool& enable)
    {
        enable = mKeyRepeatsEnabled;
    }

    void EssosInstance::update()
    {
        if (mEssosContext)
        {
            EssContextUpdateDisplay(mEssosContext);
            EssContextRunEventLoopOnce(mEssosContext);
        }
    }

    void EssosInstance::ignoreKeyInputs(bool ignore)
    {
        mKeyInputsIgnored = ignore;
    }

    bool EssosInstance::isErmEnabled()
    {
#ifdef ENABLE_ERM
        return true;
#else
        return false;
#endif // ENABLE_ERM
    }

    bool EssosInstance::setAVBlocked(std::string app, bool blockAV)
    {
        bool status = true;
#ifdef ENABLE_ERM
        status = blockAV?EssRMgrAddToBlackList(gEssRMgr, app.c_str()):EssRMgrRemoveFromBlackList(gEssRMgr, app.c_str());
        if (true == status)
        {
            gAppsAVBlacklistStatus[app] = blockAV;
        }
#endif
        return status;
    }
    void EssosInstance::getBlockedAVApplications(std::vector<std::string> &appsList)
    {
#ifdef ENABLE_ERM
        std::map<std::string, bool>::iterator appsItr = gAppsAVBlacklistStatus.begin();
        for (;appsItr != gAppsAVBlacklistStatus.end(); appsItr++)
        {
            if (true == appsItr->second)
            {
                appsList.push_back(appsItr->first);
            }
        }
#endif
    }
}
