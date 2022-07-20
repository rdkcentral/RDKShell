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

#include "rdkcompositorgroup.h"
#include "framebuffer.h"
#include "framebufferrenderer.h"

namespace RdkShell
{
    RdkCompositorGroup::RdkCompositorGroup()
    {
        mFbo = std::make_shared<FrameBuffer>(mWidth, mHeight);
    }
    
    CompositorList& RdkCompositorGroup::compositors()
    {
        return mCompositors;
    }

    void RdkCompositorGroup::draw(bool&, RdkShellRect&)
    {
        if (!mVisible) return;
        
        mFbo->bind();
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        for (auto& child : mCompositors)
        {
            bool needsHolePunch = false;
            RdkShellRect rect;
            child.compositor->draw(needsHolePunch, rect);
        }

        mFbo->unbind();
        FrameBufferRenderer::instance()->draw(mFbo, mFbo->width(), mFbo->height(), mMatrix,
                mPositionX, mPositionY, mWidth, mHeight, mOpacity);
    }

    bool RdkCompositorGroup::createDisplay(const std::string& displayName, const std::string& clientName,
                uint32_t width, uint32_t height, bool virtualDisplayEnabled, uint32_t virtualWidth, uint32_t virtualHeight)
    {
        return false;
    }
}