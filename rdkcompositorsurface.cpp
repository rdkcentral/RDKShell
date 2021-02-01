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

#include "rdkcompositorsurface.h"
#include "compositorcontroller.h"

#include <iostream>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "linuxkeys.h"
#include "rdkshell.h"

namespace RdkShell
{
    WstCompositor* RdkCompositorSurface::mMainWstContext = NULL;
    uint32_t RdkCompositorSurface::mMainCompositorWidth = 0;
    uint32_t RdkCompositorSurface::mMainCompositorHeight = 0;
    std::string RdkCompositorSurface::mMainCompositorDisplayName = "";

    void RdkCompositorSurface::createMainCompositor(const std::string& displayName, uint32_t width, uint32_t height)
    {
        if (NULL != mMainWstContext)
        {
            std::cout << "already created a main compositor..." << std::endl;
            return;
        }

        if (width > 0 && height > 0)
        {
            mMainCompositorWidth = width;
            mMainCompositorHeight = height;
        }

        mMainWstContext = WstCompositorCreate();

        bool error = false;

        if (mMainWstContext)
        {
            if (!error && !WstCompositorSetIsEmbedded(mMainWstContext, true))
            {
                error = true;
            }

            if (!error && !WstCompositorSetOutputSize(mMainWstContext, mMainCompositorWidth, mMainCompositorHeight))
            {
                error = true;
            }

            if (!error)
            {
                if (!WstCompositorSetDisplayName( mMainWstContext, displayName.c_str()))
                {
                    error = true;
                }
                mMainCompositorDisplayName = displayName;

                std::cout << "The display name is: " << mMainCompositorDisplayName << std::endl;
                
                if (!error && !WstCompositorStart(mMainWstContext))
                {
                    std::cout << "errored in starting compositor " << std::endl;
                    error= true;
                }
                std::cout << "started compositor " << mMainCompositorDisplayName << error << std::endl;
            }
        }

        if (error)
        {
            const char *detail= WstCompositorGetLastErrorDetail( mMainWstContext );
            std::cout << "error setting up the compositor: " << detail << std::endl;
            return;
        }
    }

    bool RdkCompositorSurface::createDisplay(const std::string& displayName, const std::string& clientName,
        uint32_t width, uint32_t height, bool virtualDisplayEnabled, uint32_t virtualWidth, uint32_t virtualHeight)
    {
        if (width > 0 && height > 0)
        {
            mWidth = width;
            mHeight = height;
        }
        mWstContext = WstCompositorCreateVirtualEmbedded(mMainWstContext);

        bool error = false;

        if (mWstContext)
        {
            error = !loadExtensions(mMainWstContext, clientName);

            if (!error && !WstCompositorSetIsEmbedded(mWstContext, true))
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
}
