/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2022 RDK Management
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

#pragma once

#include <string>
#include <memory>

#include "rdkshellimage.h"

namespace RdkShell
{
    class Cursor
    {
    public:
        Cursor(const std::string& fileName);

        void draw();
        void setPosition(int x, int y);
        bool load(const std::string& fileName);

        void setInactivityDuration(double inactivityDuration);
        double getInactivityDuration();

        void setSize(int width, int height);
        void getSize(int& width, int& height);

        void setOffset(int x, int y);
        void getOffset(int& x,  int& y);

    private:
        std::unique_ptr<RdkShell::Image> mCursorImage = nullptr;
        int mX;
        int mY;
        int mWidth;
        int mHeight;
        int mOffsetX; // specifies position on the cursor image that is the tip of the cursor
        int mOffsetY;
        double mInactivityDuration; // duration of inactivity after which cursor will be hidden
        double mLastUpdateTime;

        bool mIsLoaded;
    };
}
