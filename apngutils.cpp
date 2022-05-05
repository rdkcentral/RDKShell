/**

  not stated otherwise in this file or this component's LICENSE
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

    void Frame::clear()
    {
        if (data)
        {
            delete data;
	    data = NULL;
        }
	width = 0;
	height = 0;
    }

    FrameList::FrameList():mTotalTime(0),mNumPlays(0)
    {
    }

    FrameList::~FrameList()
    {
    }	    

    void FrameList::init()
    {
        mTotalTime = 0;
        mNumPlays = 0;
        mFrames.clear();
    }
    
    void FrameList::addFrame(Frame* b, double d)
    {
        FrameDetails frameDetails;
        frameDetails.frame = b;
        frameDetails.duration = d;
        mFrames.push_back(frameDetails);
        mTotalTime += d;
    }

    void FrameList::clear()
    {
        for (size_t i=0; i<mFrames.size(); i++)
	{
            mFrames[i].frame->clear();
            delete mFrames[i].frame;
	}
	mFrames.clear();
        mTotalTime = 0;
        mNumPlays = 0;
    }

    uint32_t FrameList::numFrames()
    {
        return mFrames.size();
    }

    uint32_t FrameList::numPlays()
    {
        return mNumPlays;
    }

    void FrameList::setNumPlays(uint32_t numPlays)
    {
        mNumPlays = numPlays;
    }

    Frame* FrameList::getFrameFromIndex(int frameNum)
    {
        return mFrames[frameNum].frame;
    }

    double FrameList::getFrameDuration(int frameNum)
    {
        return mFrames[frameNum].duration;
    }

    APNGData::APNGData(): frameList(), plays(0), cachedFrame(UINT32_MAX), currentFrame(UINT32_MAX), frameTime(-1)
    {
    }

    void APNGData::clear()
    {
        frameList.clear();
	plays = 0;
	cachedFrame = UINT32_MAX;
	currentFrame = UINT32_MAX;
	frameTime = -1; 
    }
}
