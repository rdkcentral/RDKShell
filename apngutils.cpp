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
    Rectangle::Rectangle(): mLeft(0), mTop(0), mRight(0), mBottom(0)
    {
    }

    Rectangle::Rectangle(int32_t l, int32_t t, int32_t r, int32_t b)
    {
        mLeft = l;
        mTop = t;
        mRight = r;
        mBottom = b;
    }

    int32_t Rectangle::left() const
    {
        return mLeft;
    }

    int32_t Rectangle::top() const 
    {
	return mTop;
    }

    int32_t Rectangle::bottom() const
    {
	return mBottom;
    }

    int32_t Rectangle::right() const
    {
	return mRight;
    }

    int32_t Rectangle::width() const
    {
        return mRight-mLeft;
    }	

    int32_t Rectangle::height() const
    {
        return mBottom-mTop;
    }

    void Rectangle::intersect(const Rectangle& r)
    {
        mLeft = Max<int32_t>(mLeft, r.mLeft);
        mTop = Max<int32_t>(mTop, r.mTop);
        mRight = Min<int32_t>(mRight, r.mRight);
        mBottom = Min<int32_t>(mBottom, r.mBottom);
    }

    FrameData::FrameData(): mBase(NULL), mWidth(0), mHeight(0), mStride(0), mUpsideDown(false)
    {
    }

    void* FrameData::base() const
    {
        return mBase;
    }

    void FrameData::setBase(void* p)
    {
        mBase = (char*) p;
    }

    int32_t FrameData::width()  const
    {
        return mWidth;
    }

    void FrameData::setWidth(int32_t width)
    {
        mWidth = width;
    }

    int32_t FrameData::height() const
    {
        return mHeight;
    }

    void FrameData::setHeight(int32_t height)
    {
        mHeight = height;
    }

    int32_t FrameData::stride() const
    {
        return mStride;
    }

    void FrameData::setStride(int32_t stride)
    {
        mStride = stride;
    }

    bool FrameData::upsideDown() const
    {
        return mUpsideDown;
    }

    void FrameData::setUpsideDown(bool upsideDown)
    {
        mUpsideDown = upsideDown;
    }

    int32_t FrameData::sizeInBytes() const
    {
        return mStride * mHeight;
    }

    void FrameData::init(int width, int height)
    {
        term();
    
        mBase = (char*) new unsigned char[width * height * 4];
    
        setBase(mBase);
        setWidth(width);
        setHeight(height);
        setStride(width * 4);
        setUpsideDown(false);
    }
    
    void FrameData::term()
    {
        delete [] mBase;
        mBase = NULL;
    }
    
    Pixel *FrameData::scanline(int32_t line) const
    {
          return (Pixel*)((uint8_t*)mBase +
                        ((mUpsideDown?(mHeight-line-1):line) * mStride));
    }

    Pixel *FrameData::pixel(int32_t x, int32_t y)
    {
        return scanline(y) + x;
    }

    Rectangle FrameData::size()
    {
        return Rectangle(0, 0, width(), height());
    }

    Rectangle FrameData::bounds()
    {
        return Rectangle(0, 0, width(), height());
    }

    void FrameData::blit(FrameData& b, int32_t destinationLeft, int32_t destinationTop,
                     int32_t destinationWidth, int32_t destinationHeight,
                     int32_t sourceLeft, int32_t sourceTop)
    {
        Rectangle sourceBounds = bounds();
        Rectangle destinationBounds = b.bounds();

        Rectangle sourceRect(sourceLeft, sourceTop, sourceLeft+destinationWidth, sourceTop+destinationHeight);
        Rectangle destinationRect(destinationLeft, destinationTop, destinationLeft+destinationWidth, destinationTop+destinationHeight);

        sourceBounds.intersect(sourceRect);
        destinationBounds.intersect(destinationRect);

        int32_t l = destinationBounds.left() + (sourceBounds.left() - sourceLeft);
        int32_t t = destinationBounds.top() + (sourceBounds.top() - sourceTop);
        int32_t w = Min<int>(sourceBounds.width(), destinationBounds.width());
        int32_t h = Min<int>(sourceBounds.height(), destinationBounds.height());

        for (int32_t y = 0; y < h; y++)
        {
            Pixel *s = pixel(sourceBounds.left(), y+sourceBounds.top());
            Pixel *se = s + w;
            Pixel *d = b.pixel(l, y+t);
            while(s < se)
            {
              *d++ = *s++;
            }
        }
    }

    void FrameData::blit(FrameData& b)
    {
        blit(b, 0, 0, width(), height(), 0, 0);
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
    
    void TimedOffscreenSequence::addBuffer(FrameData &b, double d)
    {
        entry e;
        e.mData.init(b.width(), b.height());
    
        b.blit(e.mData);
    
        e.mDuration = d;
    
        mSequence.push_back(e);
        mTotalTime += d;
    }

    void TimedOffscreenSequence::clear()
    {
        for (size_t i=0; i<mSequence.size(); i++)
	{
            mSequence[i].mData.term();
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

    FrameData & TimedOffscreenSequence::getFrameBuffer(int frameNum)
    {
        return mSequence[frameNum].mData;
    }

    double TimedOffscreenSequence::getDuration(int frameNum)
    {
        return mSequence[frameNum].mDuration;
    }

    double TimedOffscreenSequence::totalTime()
    {
        return mTotalTime;
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
