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

#include "group.h"

#include "compositorcontroller.h"
#include "framebuffer.h"
#include "framebufferrenderer.h"

#include <cstring>

namespace RdkShell
{

Group::Group(std::string name, std::function<void(bool, RdkShellRect)> onDrawFinished)
    : mChildren(), mMatrix(), mPositionX(0), mPositionY(0), mWidth(1280), mHeight(720)
    , mScaleX(1.0), mScaleY(1.0), mOpacity(1.0), mVisible(true), mHolePunch(false)
    , mOnDrawFinished(onDrawFinished)
    // NOTE: mWidth and mHeight should probably be initialized with screen resolution from essos
{
    mName = name;
    
    uint32_t width, height;
    CompositorController::getScreenResolution(width, height);
    mFbo = std::make_shared<FrameBuffer>(width, height);

    float matrix[16] = 
    {
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    };
    
    memcpy(&mMatrix, matrix, sizeof(matrix));
}

void Group::setPosition(int32_t x, int32_t y)
{
    mPositionX = x;
    mPositionY = y;
    mMatrix[12] = x;
    mMatrix[13] = y;
}

void Group::position(int32_t& x, int32_t& y)
{
    x = mPositionX;
    y = mPositionY;
}

void Group::setSize(uint32_t w, uint32_t h)
{
    mWidth = w;
    mHeight = h;
}

void Group::size(uint32_t& w, uint32_t& h)
{
    w = mWidth;
    h = mHeight;
}

void Group::setScale(double x, double y)
{
    mScaleX = x;
    mScaleY = y;
    mMatrix[0] = 1 * mScaleX;
    mMatrix[5] = 1 * mScaleY;
}

void Group::scale(double& x, double& y)
{
    x = mScaleX;
    y = mScaleY;
}


void Group::setOpacity(double o)
{
    mOpacity = o;
}

void Group::opacity(double& o)
{
    o = mOpacity;
}

void Group::setVisible(bool v)
{
    mVisible = v;
}

void Group::visible(bool& v)
{
    v = mVisible;
}

void Group::setAnimating(bool a)
{
    // NOTE: nodes added when animation is running will not have the animating hint set until next animation is scheduled
    for (auto& node : mChildren)
    {
        node->setAnimating(a);
    }
}

void Group::setHolePunch(bool hp)
{
    mHolePunch = hp;
}

void Group::holePunch(bool& hp)
{
    hp = mHolePunch;
}

void Group::draw(bool&, RdkShellRect&)
{
    if (!mVisible) return;
    
    mFbo->bind();
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    for (auto& child : mChildren)
    {
        bool needsHolePunch = false;
        RdkShellRect rect;
        child->draw(needsHolePunch, rect);

        if (child->compositor() && mOnDrawFinished)
        {
            mOnDrawFinished(needsHolePunch, rect);
        }
    }

    mFbo->unbind();
    FrameBufferRenderer::instance()->draw(mFbo, mFbo->width(), mFbo->height(), mMatrix,
            mPositionX, mPositionY, mWidth, mHeight, mOpacity);

    // FIXME: Opacity not handled
}


}