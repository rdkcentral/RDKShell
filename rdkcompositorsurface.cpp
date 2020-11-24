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
    std::vector<RdkCompositorSurface*> gAvailableRdkCompositors;
    uint32_t RdkCompositorSurface::mMainCompositorWidth = 0;
    uint32_t RdkCompositorSurface::mMainCompositorHeight = 0;
    std::string RdkCompositorSurface::mMainCompositorDisplayName = "";
    void (*WstVirtEmbUnBoundClient)( WstCompositor *wctx, int clientPID, void *userData );

    void RdkCompositorSurface::unBoundedClient( WstCompositor *wctx, int clientPID, void *userData )
    {
        if (gAvailableRdkCompositors.size() > 0)
        {
            std::vector<RdkCompositorSurface*>::iterator frontCompositor = (gAvailableRdkCompositors.begin());
            if (nullptr != *frontCompositor)
            {
                bool ret = WstCompositorVirtualEmbeddedBindClient((*frontCompositor)->mWstContext, clientPID);
                if (!ret)
                {
                    std::cout << "error setting surface to external client " << std::endl;
                }
            }
            else
            {
                std::cout << "No display available or ready for external client" << std::endl;
            }
            gAvailableRdkCompositors.erase(frontCompositor);
        }
        else
        {
            std::cout << "No display available for external client" << std::endl;
        }
    }

    RdkCompositorSurface::~RdkCompositorSurface()
    {
        for (auto it = gAvailableRdkCompositors.begin(); it != gAvailableRdkCompositors.end(); ++it)
        {
            if (*it == this)
            {
                std::cout << "destroying compositor ... " << std::endl;
                gAvailableRdkCompositors.erase(it);
                break;
            }
        }
    }

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
            const char* enableRdkShellExtendedInput = getenv("RDKSHELL_EXTENDED_INPUT_ENABLED");

            if (enableRdkShellExtendedInput)
            {
                std::string extensionInputPath = std::string(RDKSHELL_WESTEROS_PLUGIN_DIRECTORY) + "libwesteros_plugin_rdkshell_extended_input.so";
                std::cout << "attempting to load extension: " << extensionInputPath << std::endl << std::flush;
                if (!WstCompositorAddModule(mMainWstContext, extensionInputPath.c_str()))
                {
                    std::cout << "Faild to load plugin: 'libwesteros_plugin_rdkshell_extended_input.so'" << std::endl;
                    error = true;
                }
            }

            if (!WstCompositorSetIsEmbedded(mMainWstContext, true))
            {
                error = true;
            }

            if (!error && !WstCompositorSetOutputSize(mMainWstContext, mMainCompositorWidth, mMainCompositorHeight))
            {
                error = true;
            }

            if (!error && !WstCompositorSetVirtualEmbeddedUnBoundClientListener( mMainWstContext, unBoundedClient, nullptr ))
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

    bool RdkCompositorSurface::createDisplay(const std::string& displayName, uint32_t width, uint32_t height)
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
                if (!mApplicationName.empty())
                {
                    setenv("WAYLAND_DISPLAY", mMainCompositorDisplayName.c_str(), 1);
                    std::cout << "RDKShell is launching " << mApplicationName << std::endl;
                    launchApplicationInBackground();
                }
                else
                {
                    gAvailableRdkCompositors.push_back(this);
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
