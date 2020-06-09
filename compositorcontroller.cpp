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

#include "compositorcontroller.h"

#include "rdkcompositor.h"
#include "essosinstance.h"
#include "animation.h"

#include <memory>
#include <iostream>
#include <map>
#include <algorithm>

namespace RdkShell
{
    struct CompositorInfo
    {
        CompositorInfo() : name(), compositor(nullptr) {}
        std::string name;
        std::shared_ptr<RdkCompositor> compositor;
    };

    struct KeyInterceptInfo
    {
        KeyInterceptInfo() : keyCode(-1), flags(0), compositorInfo() {}
        uint32_t keyCode;
        uint32_t flags;
        struct CompositorInfo compositorInfo;
    };

    std::vector<CompositorInfo> gCompositorList;
    CompositorInfo gFocusedCompositor;

    static std::map<uint32_t, std::vector<KeyInterceptInfo>> gKeyInterceptInfoMap;

    std::string standardizeName(const std::string& clientName)
    {
        std::string displayName = clientName;
        std::transform(displayName.begin(), displayName.end(), displayName.begin(), [](unsigned char c){ return std::tolower(c); });
        return displayName;
    }
    
    bool CompositorController::moveToFront(const std::string& client)
    {
        std::string clientDisplayName = standardizeName(client);
        for (auto it = gCompositorList.begin(); it != gCompositorList.end(); ++it)
         {
            if (it->name == clientDisplayName)
            {
                auto compositorInfo = *it;
                gCompositorList.erase(it);
                gCompositorList.insert(gCompositorList.begin(), compositorInfo);
                return true;
            }
        }
        return false;
    }

    bool CompositorController::moveToBack(const std::string& client)
    {
        std::string clientDisplayName = standardizeName(client);
        for (auto it = gCompositorList.begin(); it != gCompositorList.end(); ++it)
        {
            if (it->name == clientDisplayName)
            {
                auto compositorInfo = *it;
                gCompositorList.erase(it);
                gCompositorList.push_back(compositorInfo);
                return true;
            }
        }
        return false;
    }

    bool CompositorController::moveBehind(const std::string& client, const std::string& target)
    {
        std::vector<CompositorInfo>::iterator clientIterator = gCompositorList.end();
        std::vector<CompositorInfo>::iterator targetIterator = gCompositorList.end();

        std::string clientDisplayName = standardizeName(client);
        std::string targetDisplayName = standardizeName(target);

        for (auto it = gCompositorList.begin(); it != gCompositorList.end(); ++it)
         {
            if (it->name == clientDisplayName)
            {
                clientIterator = it;
            }
            else if (it->name == targetDisplayName)
            {
                targetIterator = it;
            }
        }
        if (clientIterator != gCompositorList.end() && targetIterator != gCompositorList.end())
        {
            auto compositorInfo = *clientIterator;
            gCompositorList.erase(clientIterator);
            gCompositorList.insert(targetIterator+1, compositorInfo);
            return true;
        }
        return false;
    }

    bool CompositorController::setFocus(const std::string& client)
    {
        std::string clientDisplayName = standardizeName(client);
        for (auto compositor : gCompositorList)
        {
            if (compositor.name == clientDisplayName)
            {
                gFocusedCompositor = compositor;
                return true;
            }
        }
        return false;
    }

    bool CompositorController::kill(const std::string& client)
    {
        std::string clientDisplayName = standardizeName(client);
        for (auto it = gCompositorList.begin(); it != gCompositorList.end(); ++it)
        {
            if (it->name == clientDisplayName)
            {
                gCompositorList.erase(it);
                if (gFocusedCompositor.name == clientDisplayName)
                {
                  // this may be changed to next available compositor
                  gFocusedCompositor.name = ""; 
                  gFocusedCompositor.compositor = nullptr; 
                }
                return true;
            }
        }
        return false;
    }

    bool CompositorController::addKeyIntercept(const std::string& client, const uint32_t& keyCode, const uint32_t& flags)
    {
        std::string clientDisplayName = standardizeName(client);
        //std::cout << "key intercept added " << keyCode << " flags " << flags << std::endl;
        for (auto compositor : gCompositorList)
        {
            if (compositor.name == clientDisplayName)
            {
                struct KeyInterceptInfo info;
                info.keyCode = keyCode;
                info.flags = flags;
                info.compositorInfo = compositor;
                if (gKeyInterceptInfoMap.end() == gKeyInterceptInfoMap.find(keyCode))
                {
                  gKeyInterceptInfoMap[keyCode] = std::vector<KeyInterceptInfo>();
                  gKeyInterceptInfoMap[keyCode].push_back(info);
                }
                else
                {
                  bool isEntryAvailable = false;
                  for (int i=0; i<gKeyInterceptInfoMap[keyCode].size(); i++) {
                    struct KeyInterceptInfo& info = gKeyInterceptInfoMap[keyCode][i];
                    if ((info.flags == flags) && (info.compositorInfo.name == clientDisplayName))
                    {
                      isEntryAvailable = true;
                      break;
                    }
                  }
                  if (false == isEntryAvailable) {
                    gKeyInterceptInfoMap[keyCode].push_back(info);
                  }
                }
                return true;
            }
        }
        return false;
    }

    bool CompositorController::removeKeyIntercept(const std::string& client, const uint32_t& keyCode, const uint32_t& flags)
    {
        std::string clientDisplayName = standardizeName(client);
        for (auto compositor : gCompositorList)
        {
            if (compositor.name == clientDisplayName)
            {
                if (gKeyInterceptInfoMap.end() != gKeyInterceptInfoMap.find(keyCode))
                {
                  bool isEntryAvailable = false;
                  std::vector<KeyInterceptInfo>::iterator entryPos = gKeyInterceptInfoMap[keyCode].end();
                  for (std::vector<KeyInterceptInfo>::iterator it = gKeyInterceptInfoMap[keyCode].begin() ; it != gKeyInterceptInfoMap[keyCode].end(); ++it)
                  {
                    if (((*it).flags == flags) && ((*it).compositorInfo.name == clientDisplayName))
                    {
                      entryPos = it;
                      isEntryAvailable = true;
                      break;
                    }
                  }
                  if (true == isEntryAvailable) {
                    gKeyInterceptInfoMap[keyCode].erase(entryPos);
                    if (gKeyInterceptInfoMap[keyCode].size() == 0) {
                      gKeyInterceptInfoMap[keyCode].clear();
                    }
                  }
                }
                return true;
            }
        }
        return false;
    }

    bool CompositorController::getScreenResolution(uint32_t &width, uint32_t &height)
    {
        RdkShell::EssosInstance::instance()->resolution(width, height);
        return true;
    }

    bool CompositorController::setScreenResolution(const uint32_t width, const uint32_t height)
    {
        RdkShell::EssosInstance::instance()->setResolution(width, height);
        return true;
    }

    bool CompositorController::getClients(std::vector<std::string>& clients)
    {
        clients.clear();

        for ( const auto &client : gCompositorList )
        {
            std::string clientName = client.name;
            clients.push_back(clientName);
        }
        return true;
    }

    bool CompositorController::getZOrder(std::vector<std::string>&clients)
    {
        clients.clear();

        for ( const auto &client : gCompositorList )
        {
            std::string clientName = client.name;
            clients.push_back(clientName);
        }
        return true;
    }

    bool CompositorController::getBounds(const std::string& client, uint32_t &x, uint32_t &y, uint32_t &width, uint32_t &height)
    {
        std::string clientDisplayName = standardizeName(client);
        for (auto compositor : gCompositorList)
        {
            if (compositor.name == clientDisplayName)
            {
                int32_t xPos = 0;
                int32_t yPos = 0;
                compositor.compositor->position(xPos, yPos);
                compositor.compositor->size(width, height);
                x = (uint32_t)xPos;
                y = (uint32_t)yPos;
                return true;
            }
        }
        return false;
    }
    bool CompositorController::setBounds(const std::string& client, const uint32_t x, const uint32_t y, const uint32_t width, const uint32_t height)
    {
        std::string clientDisplayName = standardizeName(client);
        for (auto compositor : gCompositorList)
        {
            if (compositor.name == clientDisplayName)
            {
                compositor.compositor->setPosition(x,y);
                compositor.compositor->setSize(width, height);
                return true;
            }
        }
        return false;
    }

    bool CompositorController::getVisibility(const std::string& client, bool& visible)
    {
        std::string clientDisplayName = standardizeName(client);
        for (auto compositor : gCompositorList)
        {
            if (compositor.name == clientDisplayName)
            {
                compositor.compositor->visible(visible);
                return true;
            }
        }
        return false;
    }

    bool CompositorController::setVisibility(const std::string& client, const bool visible)
    {
        std::string clientDisplayName = standardizeName(client);
        for (auto compositor : gCompositorList)
        {
            if (compositor.name == clientDisplayName)
            {
                compositor.compositor->setVisible(visible);
                return true;
            }
        }
        return false;
    }

    bool CompositorController::getOpacity(const std::string& client, unsigned int& opacity)
    {
        std::string clientDisplayName = standardizeName(client);
        for (auto compositor : gCompositorList)
        {
            if (compositor.name == clientDisplayName)
            {
                double o = 1.0;
                compositor.compositor->opacity(o);
                opacity *= o;
                if (opacity > 100)
                {
                     opacity = 100;
                }
                else if (opacity < 0)
                {
                    opacity = 0;
                }
                return true;
            }
        }
        return false;
    }

    bool CompositorController::setOpacity(const std::string& client, const unsigned int opacity)
    {
        std::string clientDisplayName = standardizeName(client);
        for (auto compositor : gCompositorList)
        {
            if (compositor.name == clientDisplayName)
            {
                double o = (double)opacity / 100.0;
                compositor.compositor->setOpacity(o);
                return true;
            }
        }
        return true;
    }


    bool CompositorController::getScale(const std::string& client, double &scaleX, double &scaleY)
    {
        std::string clientDisplayName = standardizeName(client);
        for (auto compositor : gCompositorList)
        {
            if (compositor.name == clientDisplayName)
            {
                compositor.compositor->scale(scaleX, scaleY);
                return true;
            }
        }
        return false;
    }

    bool CompositorController::setScale(const std::string& client, double scaleX, double scaleY)
    {
        std::string clientDisplayName = standardizeName(client);
        for (auto compositor : gCompositorList)
        {
            if (compositor.name == clientDisplayName)
            {
                compositor.compositor->setScale(scaleX, scaleY);
                return true;
            }
        }
        return true;
    }

    void CompositorController::onKeyPress(uint32_t keycode, uint32_t flags)
    {
        //std::cout << "key press code " << keycode << " flags " << flags << std::endl;
        bool isInterceptAvailable = false;
        if (gKeyInterceptInfoMap.end() != gKeyInterceptInfoMap.find(keycode))
        {
          for (int i=0; i<gKeyInterceptInfoMap[keycode].size(); i++) {
            struct KeyInterceptInfo& info = gKeyInterceptInfoMap[keycode][i];
            if (info.flags == flags)
            {
              info.compositorInfo.compositor->onKeyPress(keycode, flags);
              isInterceptAvailable = true;
            }
          }
        }

        if ((false == isInterceptAvailable) && gFocusedCompositor.compositor)
        {
            gFocusedCompositor.compositor->onKeyPress(keycode, flags);
        }
    }

    void CompositorController::onKeyRelease(uint32_t keycode, uint32_t flags)
    {
        bool isInterceptAvailable = false;
        if (gKeyInterceptInfoMap.end() != gKeyInterceptInfoMap.find(keycode))
        {
          for (int i=0; i<gKeyInterceptInfoMap[keycode].size(); i++) {
            struct KeyInterceptInfo& info = gKeyInterceptInfoMap[keycode][i];
            if (info.flags == flags)
            {
              info.compositorInfo.compositor->onKeyRelease(keycode, flags);
              isInterceptAvailable = true;
            }
          }
        }
        if ((false == isInterceptAvailable) && gFocusedCompositor.compositor)
        {
            gFocusedCompositor.compositor->onKeyRelease(keycode, flags);
        }
    }

    bool CompositorController::createDisplay(const std::string& client, const std::string& displayName)
    {
        std::string clientDisplayName = standardizeName(client);
        std::string compositorDisplayName = displayName;
        if (displayName.empty())
        {
            compositorDisplayName = clientDisplayName;
        }
        for (auto compositor : gCompositorList)
        {
            if (compositor.name == clientDisplayName)
            {
                std::cout << "display with name " << client << " already exists\n";
                return false;
            }
        }
        CompositorInfo compositorInfo;
        compositorInfo.name = clientDisplayName;
        compositorInfo.compositor = std::make_shared<RdkCompositor>();
        uint32_t width = 0;
        uint32_t height = 0;
        RdkShell::EssosInstance::instance()->resolution(width, height);
        bool ret = compositorInfo.compositor->createDisplay(compositorDisplayName, width, height);
        if (ret)
        {
          if (gCompositorList.empty())
          {
              gFocusedCompositor = compositorInfo;
          }
          //std::cout << "display created with name: " << client << std::endl;
          gCompositorList.insert(gCompositorList.begin(), compositorInfo);
        }
        return ret;
    }

    bool CompositorController::draw()
    {
        for (std::vector<CompositorInfo>::reverse_iterator reverseIterator = gCompositorList.rbegin() ; reverseIterator != gCompositorList.rend(); reverseIterator++)
        {
            reverseIterator->compositor->draw();
        }
    }

    bool CompositorController::addAnimation(const std::string& client, double duration, std::map<std::string, RdkShellData> &animationProperties)
    {
        bool ret = false;
        RdkShell::Animation animation;
        std::string clientDisplayName = standardizeName(client);
        for (auto compositor : gCompositorList)
        {
            if (compositor.name == clientDisplayName)
            {
                int32_t x = 0;
                int32_t y = 0;
                uint32_t width = 0;
                uint32_t height = 0;
                double scaleX = 1.0;
                double scaleY = 1.0;
                if (compositor.compositor != nullptr)
                {
                    //retrieve the initial values in case they are not specified in the property set
                    compositor.compositor->position(x, y);
                    compositor.compositor->size(width, height);
                    compositor.compositor->scale(scaleX, scaleY);
                }

                for ( const auto &property : animationProperties )
                {
                    if (property.first == "x")
                    {
                        x = property.second.toInteger32();
                    }
                    else if (property.first == "y")
                    {
                        y = property.second.toInteger32();
                    }
                    else if (property.first == "w")
                    {
                        width = property.second.toUnsignedInteger32();
                    }
                    else if (property.first == "h")
                    {
                        height = property.second.toUnsignedInteger32();
                    }
                    else if (property.first == "sx")
                    {
                        scaleX = property.second.toDouble();
                    }
                    else if (property.first == "sy")
                    {
                        scaleY = property.second.toDouble();
                    }
                }

                animation.compositor = compositor.compositor;
                animation.endX = x;
                animation.endY = y;
                animation.endWidth = width;
                animation.endHeight = height;
                animation.endScaleX = scaleX;
                animation.endScaleY = scaleY;
                animation.duration = duration;
                animation.name = client;
                RdkShell::Animator::instance()->addAnimation(animation);
                ret = true;
		break;
            }
        }
        return ret;
    }

    bool CompositorController::removeAnimation(const std::string& client)
    {
        RdkShell::Animator::instance()->fastForwardAnimation(client);
        return true;
    }

    bool CompositorController::update()
    {
        RdkShell::Animator::instance()->animate();
        return true;
    }
}
