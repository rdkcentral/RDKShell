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
#include <functional>
#include <unordered_map>
#include <map>
#include <vector>

#include "rdkcompositor.h"
#include "group.h"

namespace RdkShell
{


class Scene
{
public:
    Scene();
    virtual ~Scene() = default;

    void add(std::shared_ptr<Node> node);
    void remove(std::shared_ptr<Node> node);

    std::shared_ptr<Node> find(std::string name);

    void moveInto(std::shared_ptr<Group> group, std::shared_ptr<Node> node);
    void moveToFront(std::shared_ptr<Node> node);
    void moveToBack(std::shared_ptr<Node> node);
    void moveBehind(std::shared_ptr<Node> node, std::shared_ptr<Node> target);
    
    std::shared_ptr<Group> layerDefault, layerTopmost;
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes;

    void walk(std::function<void(std::shared_ptr<Node>)> fn);

    std::vector<std::shared_ptr<Node>> clients();
    std::vector<std::shared_ptr<Node>> groups();

    void draw();
};

}