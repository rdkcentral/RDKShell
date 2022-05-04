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

#pragma once

#include <string>
#include <vector>

namespace RdkShell
{
    struct Frame
    { 
        Frame();
        char* data;
        int32_t width;
        int32_t height;
    };

    class TimedOffscreenSequence
    {
        public:
            TimedOffscreenSequence();
            ~TimedOffscreenSequence();
        
            void init();
            void addBuffer(Frame* b, double duration);
            void clear();
            uint32_t numFrames();
            uint32_t numPlays();
            void setNumPlays(uint32_t numPlays);
            Frame* getFrameFromIndex(int frameNum);
            double getDuration(int frameNum);
        
        private:
            struct entry
            {
                Frame* mData;
                double mDuration;
            };
        
            std::vector<entry> mSequence;
            double mTotalTime;
            uint32_t mNumPlays;
    };

    struct APNGData
    { 
        APNGData();
        void clear();
        TimedOffscreenSequence sequence;
        uint32_t plays;
        uint32_t cachedFrame;
        uint32_t currentFrame;
        double frameTime;
    };
}
