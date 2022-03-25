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
        void setPosition(int32_t x, int32_t y);
        bool load(const std::string& fileName);

        void setInactivityDuration(double inactivityDuration);
        double getInactivityDuration();

        void setSize(uint32_t width, uint32_t height);
        void getSize(uint32_t& width, uint32_t& height);

        void setOffset(int32_t x, int32_t y);
        void getOffset(int32_t& x,  int32_t& y);

        void show();
        void hide();

    private:
        std::unique_ptr<RdkShell::Image> mCursorImage = nullptr;
        int32_t mX;
        int32_t mY;
        uint32_t mWidth; // display size of the cursor image, initial values are taken from cursor image
        uint32_t mHeight;
        int32_t mOffsetX; // specifies position on the cursor image that is the tip of the cursor
        int32_t mOffsetY;
        double mInactivityDuration; // duration of inactivity after which cursor will be hidden
        double mLastUpdateTime;

        bool mIsVisible;
        bool mIsLoaded;
    };
}
