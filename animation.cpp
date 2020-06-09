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

#include "animation.h"
#include "animationutilities.h"

#include "rdkshell.h"
#include "essosinstance.h"

#include <iostream>

namespace RdkShell
{
    Animator* Animator::mInstance = nullptr;

    Animator::Animator()
    {
    }
    
    Animator::~Animator()
    {
    }

    Animator* Animator::instance()
    {
        if (mInstance == nullptr)
        {
            mInstance = new Animator();
        }
        return mInstance;
    }

    void Animator::animate()
    {
        uint32_t screenWidth = 0;
        uint32_t screenHeight = 0;
        RdkShell::EssosInstance::instance()->resolution(screenWidth, screenHeight);
        std::vector<std::vector<Animation>::iterator> removeAnimationList;
        for (auto it = animations.begin(); it != animations.end(); ++it)
        {
            Animation& animation = (*it);
            if (animation.compositor != nullptr)
            {
                double currentTime = RdkShell::seconds();
                double animationPosition = currentTime - animation.startTime;
                double d = 1.0;
                if (animation.endTime < currentTime)
                {
                    if (animation.compositor != nullptr)
                    {
                        animation.compositor->setAnimating(false);
                    }
                    removeAnimationList.push_back(it);
                    animationPosition = animation.duration; //without this, this is causing getting wring offset values
                }
                else
                {
                    double t1 = animationPosition/animation.duration;
                    double t2 = floor(t1);
                    t1 = t1-t2;
                    d = interpLinear(t1);
                }

                int32_t nextX = static_cast<int32_t> (animation.startX + (animation.endX-animation.startX)*d);
                int32_t nextY = static_cast<int32_t> (animation.startY + (animation.endY-animation.startY)*d);
                uint32_t nextWidth = static_cast<int32_t> (animation.startWidth + ((int32_t)(animation.endWidth-animation.startWidth))*d);
                uint32_t nextHeight = static_cast<int32_t> (animation.startHeight + ((int32_t)(animation.endHeight-animation.startHeight))*d);
                double nextScaleX = static_cast<double> (animation.startScaleX + (animation.endScaleX-animation.startScaleX)*d);
                double nextScaleY = static_cast<double> (animation.startScaleY + (animation.endScaleY-animation.startScaleY)*d);

                if (nextWidth > screenWidth)
                {
                  nextWidth = screenWidth;
                }
                if (nextHeight > screenHeight)
                {
                  nextHeight = screenHeight;
                }

                //std::cout << "start x: " << animation.startX << " y: " << animation.startY << " width: " << animation.startWidth << " height: " << animation.startHeight << std::endl;
                //std::cout << "end x: " << animation.x << " y: " << animation.y << " width: " << animation.width << " height: " << animation.height << std::endl;
                //std::cout << "next x: " << nextX << " y: " << nextY << " w: " << nextWidth << " height: " << nextHeight << std::endl << std::endl;

                animation.compositor->setPosition(nextX, nextY);
                animation.compositor->setSize(nextWidth, nextHeight);
                animation.compositor->setScale(nextScaleX, nextScaleY);
            }
        }
        for (size_t entry=0; entry<removeAnimationList.size(); entry++)
        {
          animations.erase(removeAnimationList[entry]);
        }
	    removeAnimationList.clear();
    }

    void Animator::addAnimation(Animation animation)
    {
        for (auto it = animations.begin(); it != animations.end(); ++it)
        {
            if (it->name == animation.name)
            {
                //fast forward current animation and remove from list
                auto currentAnimation = *it;
                if (currentAnimation.compositor != nullptr)
                {
                    currentAnimation.compositor->setPosition(currentAnimation.endX, currentAnimation.endY);
                    currentAnimation.compositor->setSize(currentAnimation.endWidth, currentAnimation.endHeight);
                    currentAnimation.compositor->setScale(currentAnimation.endScaleX, currentAnimation.endScaleY);
                    currentAnimation.compositor->setAnimating(false);
                }
                animations.erase(it);
                break;
            }
        }
        animation.startTime = RdkShell::seconds();
        animation.endTime = RdkShell::seconds() + animation.duration;
        animation.prepare();
        animations.push_back(animation);
    }

    void Animator::fastForwardAnimation(const std::string& name)
    {
        for (auto it = animations.begin(); it != animations.end(); ++it)
        {
            if (it->name == name)
            {
                //fast forward current animation and remove from list
                auto currentAnimation = *it;
                if (currentAnimation.compositor != nullptr)
                {
                    currentAnimation.compositor->setPosition(currentAnimation.endX, currentAnimation.endY);
                    currentAnimation.compositor->setSize(currentAnimation.endWidth, currentAnimation.endHeight);
                    currentAnimation.compositor->setScale(currentAnimation.endScaleX, currentAnimation.endScaleY);
                    currentAnimation.compositor->setAnimating(false);
                }
                animations.erase(it);
                break;
            }
        }
    }

    void Animator::stopAnimation(const std::string& name)
    {
        for (auto it = animations.begin(); it != animations.end(); ++it)
        {
            if (it->name == name)
            {
                auto currentAnimation = *it;
                if (currentAnimation.compositor != nullptr)
                {
                    currentAnimation.compositor->setAnimating(false);
                }
                animations.erase(it);
                break;
            }
        }
    }
}
