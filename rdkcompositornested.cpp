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

#include "rdkcompositornested.h"
#include "compositorcontroller.h"

#include <iostream>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "linuxkeys.h"
#include "rdkshell.h"

namespace RdkShell
{
    bool RdkCompositorNested::createDisplay(const std::string& displayName, uint32_t width, uint32_t height)
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
}
