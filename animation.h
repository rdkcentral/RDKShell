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

#include "rdkcompositor.h"

#include <memory>
#include <vector>

namespace RdkShell
{
    struct Animation
    {
        Animation() : name(), compositor(nullptr), startX(0), startY(0), startWidth(0), startHeight(0), startScaleX(1.0), startScaleY(1.0), startOpacity(1.0),
            endX(0), endY(0), endWidth(0), endHeight(0), endOpacity(1.0), duration(0), startTime(0), endTime(0), tween("linear"), delay(0) {}
        void setDestinationBounds(int32_t destinationX, int32_t destinationY, 
                                    uint32_t destinationWidth, uint32_t destinationHeight) 
        {
            endX = destinationX;
            endY = destinationY;
            endWidth = destinationWidth;
            endHeight = destinationHeight;
        }

        void prepare()
        {
            if (compositor != nullptr)
            {
                compositor->position(startX ,startY);
                compositor->size(startWidth, startHeight);
                compositor->setAnimating(true);
                compositor->scale(startScaleX, startScaleY);
                compositor->opacity(startOpacity);
            }
        }

        std::string name;
        std::shared_ptr<Node> compositor;
        int32_t startX;
        int32_t startY;
        uint32_t startWidth;
        uint32_t startHeight;
        double startScaleX;
        double startScaleY;
        double startOpacity;
        int32_t endX;
        int32_t endY;
        uint32_t endWidth;
        uint32_t endHeight;
        double endScaleX;
        double endScaleY;
        double endOpacity;

        double duration;
        double startTime;
        double endTime;
        std::string tween;
        double delay;
    };

    class Animator
    {
        public:
        static Animator* instance();
        void animate();
        void addAnimation(Animation animation);
        void fastForwardAnimation(const std::string& name);
        void stopAnimation(const std::string& name);


        private:
        Animator();
        ~Animator();

        static Animator* mInstance;
        std::vector<Animation> animations;

    };
}
