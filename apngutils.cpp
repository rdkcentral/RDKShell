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

#include "apngutils.h"

namespace RdkShell
{
    Frame::Frame(): data(NULL), width(0), height(0)
    {
    }

    TimedOffscreenSequence::TimedOffscreenSequence():mTotalTime(0),mNumPlays(0)
    {
    }

    TimedOffscreenSequence::~TimedOffscreenSequence()
    {
    }	    

    void TimedOffscreenSequence::init()
    {
        mTotalTime = 0;
        mNumPlays = 0;
        mSequence.clear();
    }
    
    void TimedOffscreenSequence::addBuffer(Frame* b, double d)
    {
        entry e;
        e.mData = b;
        e.mDuration = d;
        mSequence.push_back(e);
        mTotalTime += d;
    }

    void TimedOffscreenSequence::clear()
    {
        for (size_t i=0; i<mSequence.size(); i++)
	{
            mSequence[i].mData->term();
            delete mSequence[i].mData;
	}
	mSequence.clear();
        mTotalTime = 0;
        mNumPlays = 0;
    }

    uint32_t TimedOffscreenSequence::numFrames()
    {
        return mSequence.size();
    }

    uint32_t TimedOffscreenSequence::numPlays()
    {
        return mNumPlays;
    }

    void TimedOffscreenSequence::setNumPlays(uint32_t numPlays)
    {
        mNumPlays = numPlays;
    }

    Frame* TimedOffscreenSequence::getFrameFromIndex(int frameNum)
    {
        return mSequence[frameNum].mData;
    }

    double TimedOffscreenSequence::getDuration(int frameNum)
    {
        return mSequence[frameNum].mDuration;
    }

    APNGData::APNGData(): sequence(), plays(0), cachedFrame(UINT32_MAX), currentFrame(UINT32_MAX), frameTime(-1)
    {
    }

    void APNGData::clear()
    {
        sequence.clear();
	plays = 0;
	cachedFrame = UINT32_MAX;
	currentFrame = UINT32_MAX;
	frameTime = -1; 
    }
}
