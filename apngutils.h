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

    #define PX_LITTLEENDIAN_PIXELS
    #define PX_LITTLEENDIAN_RGBA_PIXELS

    template <typename t>
    inline t Min(t a1, t a2)
    {
      return (a1 < a2)?a1:a2;
    }
    
    template <typename t>
    inline t Max(t a1, t a2)
    {
      return (a1 > a2)?a1:a2;
    }

    struct Pixel 
    {
      inline Pixel() {}
      inline Pixel(uint32_t _u)      {u=_u;}
    	inline Pixel(const Pixel& p) {u=p.u;}
      
      inline Pixel(uint8_t _r,uint8_t _g,uint8_t _b,uint8_t _a=255)     
        {r=_r;g=_g;b=_b;a=_a;}
    
    	Pixel& operator=(const Pixel& p) {u = p.u;return *this;}
    
      union 
      {
        struct 
        {
    #ifdef PX_LITTLEENDIAN_PIXELS
    #ifdef PX_LITTLEENDIAN_RGBA_PIXELS
          uint8_t r,g,b,a;
    #else
          uint8_t b,g,r,a;
    #endif
    #else
          uint8_t b: 8;   // LSB
          uint8_t g: 8;
          uint8_t r: 8;
          uint8_t a: 8;   // MSB
    #endif
        };
        uint32_t u;
        uint8_t  bytes[4];
      };//UNION
    };

    class Rectangle
    {
        public:
            inline Rectangle();
            inline Rectangle(int32_t l, int32_t t, int32_t r, int32_t b);
            inline int32_t left() const;
            inline int32_t top() const;
            inline int32_t bottom() const;
            inline int32_t right() const;
            inline int32_t width() const;
            inline int32_t height() const;
            void intersect(const Rectangle& r);
        
        private:
            int32_t mLeft, mTop, mRight, mBottom;
    };

    class FrameData
    { 
       public:
           FrameData();

           void* base() const;
           void setBase(void* p);

           int32_t width()  const;
           void setWidth(int32_t width);

           int32_t height() const;
           void setHeight(int32_t height);

           int32_t stride() const;
           void setStride(int32_t stride);

           bool upsideDown() const;
           void setUpsideDown(bool upsideDown);

           int32_t sizeInBytes() const;
           void init(int width, int height);
           void term();
           void blit(FrameData& b, int32_t dstLeft, int32_t dstTop,
              int32_t dstWidth, int32_t dstHeight,
              int32_t srcLeft, int32_t srcTop);
           void blit(FrameData& b);
           Pixel *scanline(int32_t line) const;
           Pixel *pixel(int32_t x, int32_t y);
           Rectangle size();
           Rectangle bounds();

       private:
           char* mBase;
           int32_t mWidth;
           int32_t mHeight;
           int32_t mStride;
           bool mUpsideDown;
    };

    class TimedOffscreenSequence
    {
        public:
            TimedOffscreenSequence();
            ~TimedOffscreenSequence();
        
            void init();
            void addBuffer(FrameData &b, double duration);
            void clear();
            uint32_t numFrames();
            uint32_t numPlays();
            void setNumPlays(uint32_t numPlays);
            FrameData &getFrameBuffer(int frameNum);
            double getDuration(int frameNum);
            double totalTime();
        
        private:
            struct entry
            {
                FrameData mData;
                double mDuration;
            };
        
            std::vector<entry> mSequence;
            double mTotalTime;
            uint32_t mNumPlays;
      
    }; // CLASS - TimedOffscreenSequence

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
