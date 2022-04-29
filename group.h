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

#include <list>
#include <functional>

#include "node.h"

namespace RdkShell
{

class FrameBuffer;

class Group : public Node
{
public:
    Group(std::string name, std::function<void(bool, RdkShellRect)> onDrawFinished = nullptr);

    std::shared_ptr<Group> group() override { return shared_from(this); }
    virtual ~Group() = default;

    std::list<std::shared_ptr<Node>>& children() { return mChildren; }


    void draw(bool &needsHolePunch, RdkShellRect& rect) override;

    void setPosition(int32_t x, int32_t y) override;
    void position(int32_t& x, int32_t& y) override;

    void setSize(uint32_t w, uint32_t h) override;
    void size(uint32_t& w, uint32_t& h) override;

    void setScale(double x, double y) override;
    void scale(double& x, double& y) override;

    void setOpacity(double o) override;
    void opacity(double& o) override;

    void setVisible(bool v) override;
    void visible(bool& v) override;

    void setAnimating(bool a) override;

    void setHolePunch(bool hp) override;
    void holePunch(bool& hp) override;

protected:
    std::list<std::shared_ptr<Node>> mChildren;
    std::shared_ptr<FrameBuffer> mFbo;
    float mMatrix[16];

    int32_t mPositionX, mPositionY;
    uint32_t mWidth, mHeight;
    double mScaleX, mScaleY;
    double mOpacity;
    bool mVisible;
    bool mHolePunch;

    std::function<void(bool, RdkShellRect)> mOnDrawFinished;
};

}