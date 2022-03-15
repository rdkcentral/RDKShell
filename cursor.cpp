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

#include "cursor.h"
#include "rdkshell.h"
#include "rdkshellimage.h"
#include "essosinstance.h"
#include "logger.h"


namespace RdkShell
{
    const int DEFAULT_CURSOR_SIZE = 64;
    const double DEFAULT_INACTIVITY_DURATION = 5.0;

    Cursor::Cursor(const std::string& fileName)
        : mIsLoaded(false), mX(0), mY(0), mWidth(DEFAULT_CURSOR_SIZE), mHeight(DEFAULT_CURSOR_SIZE)
        , mInactivityDuration(DEFAULT_INACTIVITY_DURATION), mOffsetX(0), mOffsetY(0), mLastUpdateTime(0.0)
    {
        load(fileName);
    }

    bool Cursor::load(const std::string& cursorImageName)
    {
        if (cursorImageName.empty())
        {
            mIsLoaded = false;
            Logger::log(LogLevel::Information,  "cursor image name is empty");
            return mIsLoaded;
        }

        mCursorImage = std::make_unique<RdkShell::Image>();
        mIsLoaded = mCursorImage->loadLocalFile(cursorImageName);
        if (!mIsLoaded)
        {
            RdkShell::Logger::log(RdkShell::LogLevel::Error, "error loading cursor image: '%s'", cursorImageName.c_str());
        }

        RdkShell::Logger::log(RdkShell::LogLevel::Error, "finish lading cursor '%s', mIsLoaded: %d", cursorImageName.c_str(), mIsLoaded);

        return mIsLoaded;
    }

    void Cursor::setInactivityDuration(double duration)
    {
        mInactivityDuration = duration;
    }

    double Cursor::getInactivityDuration()
    {
        return mInactivityDuration;
    }

    void Cursor::setSize(int width, int height)
    {
        mWidth = width;
        mHeight = height;
    }

    void Cursor::getSize(int& width, int& height)
    {
        width = mWidth;
        height = mHeight;
    }

    void Cursor::setOffset(int x, int y)
    {
        mOffsetX = x;
        mOffsetY = y;
    }

    void Cursor::getOffset(int& x,  int& y)
    {
        x = mOffsetX;
        y = mOffsetY;
    }

    void Cursor::setPosition(int x, int y)
    {
        Logger::log(LogLevel::Information,  "Cursor::setPosition(%d, %d), mIsLoaded: %d, cursor: %p", x, y, mIsLoaded, this);
     
        uint32_t screenWidth = 0;
        uint32_t screenHeight = 0;
        RdkShell::EssosInstance::instance()->resolution(screenWidth, screenHeight);
        
        mX = x;
        mY = screenHeight - y;
        mLastUpdateTime = RdkShell::seconds();
    }

    void Cursor::draw()
    {
        if (!mIsLoaded)
            return;

        if (RdkShell::seconds() - mLastUpdateTime < mInactivityDuration)
        {
            mCursorImage->setBounds(mX - mOffsetX, mY - mHeight + mOffsetY, mWidth, mHeight);
            mCursorImage->draw(true);
        }
    }

}