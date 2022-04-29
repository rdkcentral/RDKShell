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

#include <memory>
#include <cstdint>
#include <string>
#include "rdkshellrect.h"

namespace RdkShell
{

    struct NodeCommon
    {
        virtual void draw(bool &needsHolePunch, RdkShellRect& rect) = 0;

        virtual void setPosition(int32_t x, int32_t y) = 0;
        virtual void position(int32_t& x, int32_t& y) = 0;

        virtual void setSize(uint32_t w, uint32_t h) = 0;
        virtual void size(uint32_t& w, uint32_t& h) = 0;

        virtual void setScale(double x, double y) = 0;
        virtual void scale(double& x, double& y) = 0;

        virtual void setOpacity(double o) = 0;
        virtual void opacity(double& o) = 0;

        virtual void setVisible(bool v) = 0;
        virtual void visible(bool& v) = 0;

        virtual void setAnimating(bool a) = 0;

        virtual void setHolePunch(bool hp) = 0;
        virtual void holePunch(bool& hp) = 0;

        virtual ~NodeCommon() = default;
    };

    class RdkCompositor;
    class Group;

    class Node : public NodeCommon, public std::enable_shared_from_this<Node>
    {
    public:
        std::string name() { return mName; }
        void setName(std::string name) { mName = name; }
        
        std::shared_ptr<Group> parent() { return mParent.lock(); }

        std::shared_ptr<Group> root();

        virtual std::shared_ptr<RdkCompositor> compositor() { return nullptr; }
        virtual std::shared_ptr<Group> group() { return nullptr; }

        virtual ~Node() = default;
        friend class Scene;

    protected:
        std::string mName;
        std::weak_ptr<Group> mParent;
        
        template <typename T>
        inline std::shared_ptr<T> shared_from(T* t)
        {
            return std::static_pointer_cast<T>(shared_from_this());
        }
    };

}