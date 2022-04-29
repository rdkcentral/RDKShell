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

#include "scene.h"

#include <algorithm>

namespace RdkShell
{

    namespace
    {

        void _walk(std::shared_ptr<Node> node, std::function<void(std::shared_ptr<Node>)> fn)
        {
            auto group = node->group();
            
            if (group) for (auto child : group->children()) _walk(child, fn);

            fn(node);
        }

        std::string standardizeName(const std::string& clientName)
        {
            std::string displayName = clientName;
            std::transform(displayName.begin(), displayName.end(), displayName.begin(), [](unsigned char c){ return std::tolower(c); });
            return displayName;
        }

    }

    Scene::Scene() 
        : layerDefault(std::make_shared<Group>(""))
        , layerTopmost(std::make_shared<Group>(""))
    {
    }

    std::shared_ptr<Node> Scene::find(std::string name)
    {
        auto it = nodes.find(standardizeName(name));
        return it != nodes.end() ? it->second : nullptr;
    }

    void Scene::moveToFront(std::shared_ptr<Node> node)
    {
        if (!node) return; // Error: Node not found

        auto nodeParent = node->parent();
        if (!nodeParent) return; // Error: Node has no parent

        auto& nodeList = nodeParent->children();
        nodeList.remove(node);
        nodeList.insert(nodeList.end(), node);
    }

    void Scene::moveToBack(std::shared_ptr<Node> node)
    {
        if (!node) return; // Error: Node not found

        auto nodeParent = node->parent();
        if (!nodeParent) return; // Error: Node has no parent

        auto& nodeList = nodeParent->children();
        nodeList.remove(node);
        nodeList.insert(nodeList.begin(), node);
    }

    void Scene::moveBehind(std::shared_ptr<Node> node, std::shared_ptr<Node> target)
    {
        if (!node) return; // Error: Node not found
        if (!target) return; // Error: Target not found
        if (node == target) return; // Error: Cannot move behind itself

        auto nodeParent = node->parent();
        if (!nodeParent) return; // Error: Node has no parent
        if (nodeParent != target->parent()) return; // Error: Nodes not in the same context

        auto& nodeList = nodeParent->children();
        nodeList.remove(node);
        nodeList.insert(std::find(nodeList.begin(), nodeList.end(), target), node);
    }

    void Scene::moveInto(std::shared_ptr<Group> group, std::shared_ptr<Node> node)
    {
        if (!node) return; // Error: Cannot add null node

        nodes[node->name()] = node;
        
        if (!node->mParent.expired()) node->parent()->children().remove(node);
        node->mParent = group;
        group->children().push_back(node);
    }

    void Scene::add(std::shared_ptr<Node> node)
    {
        moveInto(layerDefault, node);
    }

    void Scene::remove(std::shared_ptr<Node> node)
    {
        auto& siblings = node->parent()->children();
        auto it = std::find(siblings.begin(), siblings.end(), node);
        
        auto group = node->group();
        if (group) // When a group is destroyed all its children are extracted as if the group never existed
        {
            for (auto&& child : group->children())
            {
                siblings.insert(it, child);
            }
        }

        siblings.erase(it);
        nodes.erase(node->name());
    }

    void Scene::walk(std::function<void(std::shared_ptr<Node>)> fn)
    {
        for (auto node : layerDefault->children()) _walk(node, fn);
        for (auto node : layerTopmost->children()) _walk(node, fn);
    }

    std::vector<std::shared_ptr<Node>> Scene::clients()
    {
        std::vector<std::shared_ptr<Node>> nodes;
        
        walk([&nodes](auto node) {
            if (node->compositor()) nodes.push_back(node);
        });

        return nodes;
    }

    std::vector<std::shared_ptr<Node>> Scene::groups()
    {
        std::vector<std::shared_ptr<Node>> nodes;
        
        walk([&nodes](auto node) {
            if (node->group()) nodes.push_back(node);
        });

        return nodes;
    }

    void Scene::draw()
    {
        bool needsHolePunch;
        RdkShellRect rect;
        for (auto node : layerDefault->children()) node->draw(needsHolePunch, rect);
        for (auto node : layerTopmost->children()) node->draw(needsHolePunch, rect);
    }

}