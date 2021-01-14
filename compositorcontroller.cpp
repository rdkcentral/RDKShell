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

#include "essosinstance.h"
#include "animation.h"
#include "rdkshell.h"
#include "application.h"
#include "logger.h"
#include "linuxkeys.h"
#include "eastereggs.h"
#include "rdkcompositornested.h"
#include "rdkcompositorsurface.h"
#include "string.h"
#include "rdkshellimage.h"
#include <iostream>
#include <map>

#define RDKSHELL_ANY_KEY 65536
#define RDKSHELL_DEFAULT_INACTIVITY_TIMEOUT_IN_SECONDS 15*60
#define RDKSHELL_WILDCARD_KEY_CODE 255

namespace RdkShell
{
    struct KeyListenerInfo
    {
        KeyListenerInfo() : keyCode(-1), flags(0), activate(false), propagate(true) {}
        uint32_t keyCode;
        uint32_t flags;
        bool activate;
        bool propagate;
    };

    struct CompositorInfo
    {
        CompositorInfo() : name(), compositor(nullptr), eventListeners(), mimeType() {}
        std::string name;
        std::shared_ptr<RdkCompositor> compositor;
        std::map<uint32_t, std::vector<KeyListenerInfo>> keyListenerInfo;
        std::vector<std::shared_ptr<RdkShellEventListener>> eventListeners;
        std::string mimeType;
    };

    struct KeyInterceptInfo
    {
        KeyInterceptInfo() : keyCode(-1), flags(0), compositorInfo() {}
        uint32_t keyCode;
        uint32_t flags;
        struct CompositorInfo compositorInfo;
    };

    enum RdkShellCompositorType
    {
        NESTED,
        SURFACE
    };

    std::vector<CompositorInfo> gCompositorList;
    CompositorInfo gFocusedCompositor;
    std::vector<std::shared_ptr<RdkCompositor>> gPendingKeyUpListeners;

    static std::map<uint32_t, std::vector<KeyInterceptInfo>> gKeyInterceptInfoMap;

    bool gEnableInactivityReporting = false;
    double gInactivityIntervalInSeconds = RDKSHELL_DEFAULT_INACTIVITY_TIMEOUT_IN_SECONDS;
    double gLastKeyEventTime = RdkShell::seconds();
    double gNextInactiveEventTime = RdkShell::seconds() + gInactivityIntervalInSeconds;
    std::shared_ptr<RdkShellEventListener> gRdkShellEventListener;
    double gLastKeyPressStartTime = 0.0;
    RdkShellCompositorType gRdkShellCompositorType = NESTED;
    std::shared_ptr<RdkShell::Image> gSplashImage = nullptr;
    bool gShowSplashImage = false;
    uint32_t gSplashDisplayTimeInSeconds = 0;
    double gSplashStartTime = 0;
    std::shared_ptr<RdkShell::Image> gWaterMarkImage = nullptr;
    bool gShowWaterMarkImage = false;
    uint32_t gPowerKeyCode = 0;
    bool gPowerKeyEnabled = false;

    std::string standardizeName(const std::string& clientName)
    {
        std::string displayName = clientName;
        std::transform(displayName.begin(), displayName.end(), displayName.begin(), [](unsigned char c){ return std::tolower(c); });
        return displayName;
    }

    void sendApplicationEvent(std::shared_ptr<RdkShellEventListener>& listener, const std::string& eventName, const std::string& client)
    {
         if (eventName.compare(RDKSHELL_EVENT_APPLICATION_LAUNCHED) == 0)
         {
                 listener->onApplicationLaunched(client);
         }
         else if(eventName.compare(RDKSHELL_EVENT_APPLICATION_TERMINATED) == 0)
         {
                 listener->onApplicationTerminated(client);
         }
         else if(eventName.compare(RDKSHELL_EVENT_APPLICATION_CONNECTED) == 0)
         {
                 listener->onApplicationConnected(client);
         }
         else if(eventName.compare(RDKSHELL_EVENT_APPLICATION_DISCONNECTED) == 0)
         {
                 listener->onApplicationDisconnected(client);
         }
         else if(eventName.compare(RDKSHELL_EVENT_APPLICATION_FIRST_FRAME) == 0)
         {
                 listener->onApplicationFirstFrame(client);
         }
         else if(eventName.compare(RDKSHELL_EVENT_APPLICATION_SUSPENDED) == 0)
         {
                 listener->onApplicationSuspended(client);
         }
         else if(eventName.compare(RDKSHELL_EVENT_APPLICATION_RESUMED) == 0)
         {
                 listener->onApplicationResumed(client);
         }
    }
    
    bool interceptKey(uint32_t keycode, uint32_t flags, uint64_t metadata, bool isPressed)
    {
      bool ret = false;
      if (gKeyInterceptInfoMap.end() != gKeyInterceptInfoMap.find(keycode))
      {
        for (int i=0; i<gKeyInterceptInfoMap[keycode].size(); i++) {
          struct KeyInterceptInfo& info = gKeyInterceptInfoMap[keycode][i];
          if (info.flags == flags)
          {
            Logger::log(Debug, "Key %d intercepted by client %s", keycode, info.compositorInfo.name.c_str());
            if (isPressed)
            {
              info.compositorInfo.compositor->onKeyPress(keycode, flags, metadata);
            }
            else
            {
              info.compositorInfo.compositor->onKeyRelease(keycode, flags, metadata);
            }
            ret = true;
          }
        }
      }
      return ret;
    }

    void evaluateKeyListeners(struct CompositorInfo& compositor, uint32_t keycode, uint32_t flags, bool& foundlistener, bool& activate, bool& propagate)
    {
        std::map<uint32_t, std::vector<KeyListenerInfo>>& keyListenerInfo = compositor.keyListenerInfo;

        if (keyListenerInfo.end() != keyListenerInfo.find(keycode))
        {
          for (size_t i=0; i<keyListenerInfo[keycode].size(); i++)
          {
            struct KeyListenerInfo& info = keyListenerInfo[keycode][i];

            if (info.flags == flags)
            {
              foundlistener  = true;
              activate = info.activate;
              propagate = info.propagate;
              break;
            }
          }
        }

        // handle wildcard if no listener found
        if ((false == foundlistener) && (keyListenerInfo.find(RDKSHELL_ANY_KEY) != keyListenerInfo.end()))
        {
          struct KeyListenerInfo& info = keyListenerInfo[RDKSHELL_ANY_KEY][0];
          foundlistener  = true;
          activate = info.activate;
          propagate = info.propagate;
        }
    }

    void bubbleKey(uint32_t keycode, uint32_t flags, uint64_t metadata, bool isPressed)
    {
        std::vector<CompositorInfo>::iterator compositorIterator = gCompositorList.begin();
        for (compositorIterator = gCompositorList.begin();  compositorIterator != gCompositorList.end(); compositorIterator++)
        {
          if (compositorIterator->name == gFocusedCompositor.name)
          {
            break;
          }
        }

        bool activateCompositor = false, propagateKey = true, foundListener = false;
        bool stopPropagation = false;
        bool isFocusedCompositor = true;
        while (compositorIterator != gCompositorList.end())
        {
          activateCompositor = false;
          propagateKey = true;
          foundListener = false;
          evaluateKeyListeners(*compositorIterator, keycode, flags, foundListener, activateCompositor, propagateKey);

          if ((false == isFocusedCompositor) && (true == foundListener))
          {
            Logger::log(Debug, "Key %d sent to listener %s", keycode, compositorIterator->name.c_str());
            if (isPressed)
            {
              compositorIterator->compositor->onKeyPress(keycode, flags, metadata);
              gPendingKeyUpListeners.push_back(compositorIterator->compositor);
            }
            else
            {
              compositorIterator->compositor->onKeyRelease(keycode, flags, metadata);
            }
          }
          isFocusedCompositor = false;
          if (activateCompositor)
          {
              if (gFocusedCompositor.name != compositorIterator->name)
              {
                  std::string previousFocusedClient = !gFocusedCompositor.name.empty() ? gFocusedCompositor.name:"none";
                  std::cout << "rdkshell_focus bubbleKey: the focused client is now " << (*compositorIterator).name << ".  previous: " << previousFocusedClient << std::endl;
                  if ((gFocusedCompositor.compositor) && (gFocusedCompositor.compositor->isKeyPressed()))
                  {
                      gPendingKeyUpListeners.push_back(gFocusedCompositor.compositor);
                  }
                  gFocusedCompositor = *compositorIterator;

                  if (gRdkShellEventListener)
                  {
                      gRdkShellEventListener->onApplicationActivated(gFocusedCompositor.name);
                  }
              }
          }

          //propagate is false, stopping here
          if (false == propagateKey)
          {
            break;
          }
          compositorIterator++;
        }
    }

    std::shared_ptr<RdkCompositor> CompositorController::getCompositor(const std::string& displayName)
    {
        auto it = std::find_if(std::begin(gCompositorList), std::end(gCompositorList),
                [displayName](CompositorInfo& info)
                    {
                        std::string compositorDisplayName;
                        info.compositor->displayName(compositorDisplayName);
                        return compositorDisplayName == displayName; 
                    });

        if (it == std::end(gCompositorList))
        {
            return nullptr;
        }

        return it->compositor;
    }

    void CompositorController::initialize()
    {
        static bool sCompositorInitialized = false;
        if (sCompositorInitialized)
            return;

        char const *rdkshellPowerKey = getenv("RDKSHELL_POWER_KEY_CODE");

        if (rdkshellPowerKey)
        {
            int keyCode = atoi(rdkshellPowerKey);
            if (keyCode > 0)
            {
                gPowerKeyCode = (uint32_t)keyCode;
            }
        }
        std::cout << "the power key is set to " <<  gPowerKeyCode << std::endl;

        char const *rdkshellPowerKeyEnable = getenv("RDKSHELL_ENABLE_POWER_KEY");

        if (rdkshellPowerKeyEnable)
        {
            int powerValue = atoi(rdkshellPowerKeyEnable);
            if (powerValue > 0)
            {
                gPowerKeyEnabled = true;
            }
        }

        std::cout << "power key support enabled: " << gPowerKeyEnabled << std::endl;
 
        char const *rdkshellCompositorType = getenv("RDKSHELL_COMPOSITOR_TYPE");

        if (NULL == rdkshellCompositorType)
        {
            std::cout << "compositor type is empty, setting to nested by default " << std::endl;
            return;
        }
         
        if (strcmp(rdkshellCompositorType, "surface") == 0)
        {
            gRdkShellCompositorType = SURFACE;
            uint32_t width = 0;
            uint32_t height = 0;
            RdkShell::EssosInstance::instance()->resolution(width, height);
            RdkCompositorSurface::createMainCompositor("rdkshell_display", width, height);
        }
        else if (strcmp(rdkshellCompositorType, "nested") != 0)
        {
            std::cout << "invalid compositor type, setting to nested by default " << std::endl;
        }

        sCompositorInitialized = true;
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

        CompositorInfo compositorInfo;
        for (auto it = gCompositorList.begin(); it != gCompositorList.end(); ++it)
        {
            if (it->name == clientDisplayName)
            {
                clientIterator = it;
                break;
            }
        }

        if (clientIterator != gCompositorList.end())
        {
            compositorInfo = *clientIterator;
            gCompositorList.erase(clientIterator);

            for (auto it = gCompositorList.begin(); it != gCompositorList.end(); ++it)
            {
                if (it->name == targetDisplayName)
                {
                    targetIterator = it;
                    break;
                }
            }
            if (targetIterator != gCompositorList.end())
            {
                gCompositorList.insert(targetIterator+1, compositorInfo);
                return true;
            }
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
                std::string previousFocusedClient = !gFocusedCompositor.name.empty() ? gFocusedCompositor.name:"none";
                std::cout << "rdkshell_focus setFocus: the focused client is now " << compositor.name << ".  previous: " << previousFocusedClient << std::endl;
                if ((gFocusedCompositor.compositor) && (gFocusedCompositor.compositor->isKeyPressed()))
                {
                    gPendingKeyUpListeners.push_back(gFocusedCompositor.compositor);
                }
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
                RdkShell::Animator::instance()->stopAnimation(clientDisplayName);

                // cleanup key intercepts
		std::vector<std::map<uint32_t, std::vector<KeyInterceptInfo>>::iterator> emptyKeyCodeEntries;
                std::map<uint32_t, std::vector<KeyInterceptInfo>>::iterator entry = gKeyInterceptInfoMap.begin();
                while(entry != gKeyInterceptInfoMap.end())
                {
                    std::vector<KeyInterceptInfo>& interceptMap = entry->second;
                    std::vector<KeyInterceptInfo>::iterator interceptMapEntry=interceptMap.begin();
                    while (interceptMapEntry != interceptMap.end())
                    {
                        if ((*interceptMapEntry).compositorInfo.name == clientDisplayName)
                        {
                          interceptMapEntry = interceptMap.erase(interceptMapEntry);
                        }
                        else
                        {
                          interceptMapEntry++;
                        }
                    }
                    if (interceptMap.size() == 0)
                    {
                       entry = gKeyInterceptInfoMap.erase(entry);
                    }
                    else
                    {
                      entry++;
                    }
                }

                // cleanup key listeners
                for (std::map<uint32_t, std::vector<KeyListenerInfo>>::iterator iter = it->keyListenerInfo.begin(); iter != it->keyListenerInfo.end(); iter++)
                {
                  iter->second.clear();
                }
                it->keyListenerInfo.clear();
                it->eventListeners.clear();
                gCompositorList.erase(it);
                if (gFocusedCompositor.name == clientDisplayName)
                {
                  // this may be changed to next available compositor
                  gFocusedCompositor.name = "";
                  gFocusedCompositor.compositor = nullptr;
                  std::cout << "rdkshell_focus kill: the focused client has been killed: " << clientDisplayName  << ".  there is no focused client.\n";
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
        if (keyCode == RDKSHELL_WILDCARD_KEY_CODE)
        {
            std::string clientDisplayName = standardizeName(client);
            for (std::map<uint32_t, std::vector<KeyInterceptInfo>>::iterator keyInterceptIterator = gKeyInterceptInfoMap.begin(); keyInterceptIterator != gKeyInterceptInfoMap.end(); keyInterceptIterator++)
            {
                std::vector<KeyInterceptInfo>& interceptInfo = keyInterceptIterator->second;
                std::vector<KeyInterceptInfo>::iterator interceptInfoIterator = interceptInfo.begin();
                while(interceptInfoIterator != interceptInfo.end())
                {
                    if ((*interceptInfoIterator).compositorInfo.name == client)
                    {
                         interceptInfoIterator = interceptInfo.erase(interceptInfoIterator);
                    }
                }
            }
        }
        if (client == "*")
        {
            std::vector<std::vector<KeyInterceptInfo>::iterator> keyMapEntries;
            std::map<uint32_t, std::vector<KeyInterceptInfo>>::iterator it = gKeyInterceptInfoMap.find(keyCode);
            if (it != gKeyInterceptInfoMap.end())
            {
              std::vector<KeyInterceptInfo>::iterator entry = gKeyInterceptInfoMap[keyCode].begin();
              while(entry != gKeyInterceptInfoMap[keyCode].end())
              {
                  if ((*entry).flags == flags)
                  {
                    if (((*entry).compositorInfo.compositor) && ((*entry).compositorInfo.compositor->isKeyPressed()))
                    {
                        gPendingKeyUpListeners.push_back((*entry).compositorInfo.compositor);
                    }
                    entry = gKeyInterceptInfoMap[keyCode].erase(entry);
                  }
                  else
                  {
                    entry++;
                  }
              }
              if ( gKeyInterceptInfoMap[keyCode].size() == 0)
              {
                 gKeyInterceptInfoMap.erase(keyCode);
              }
            }
            return true;
        }

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
                    if (((*entryPos).compositorInfo.compositor) && ((*entryPos).compositorInfo.compositor->isKeyPressed()))
                    {
                        gPendingKeyUpListeners.push_back((*entryPos).compositorInfo.compositor);
                    }
                    gKeyInterceptInfoMap[keyCode].erase(entryPos);
                    if (gKeyInterceptInfoMap[keyCode].size() == 0) {
                      gKeyInterceptInfoMap.erase(keyCode);
                    }
                  }
                }
                return true;
            }
        }
        return false;
    }

    bool CompositorController::addKeyListener(const std::string& client, const uint32_t& keyCode, const uint32_t& flags, std::map<std::string, RdkShellData> &listenerProperties)
    {
        std::string clientDisplayName = standardizeName(client);
        bool activate = false, propagate = true;
        for ( const auto &property : listenerProperties)
        {
          if (property.first == "activate")
          {
            activate = property.second.toBoolean();
          }
          else if (property.first == "propagate")
          {
            propagate = property.second.toBoolean();
          }
        }
        std::cout << "key listener added client: " << client.c_str() << " activate: " << activate << " propagate: " << propagate
                  << " RDKShell keyCode: " << keyCode << " flags: " << flags << std::endl;

        for (std::vector<CompositorInfo>::iterator it = gCompositorList.begin(); it != gCompositorList.end(); ++it)
        {
            if (it->name == clientDisplayName)
            {
                struct KeyListenerInfo info;
                info.keyCode = keyCode;
                info.flags = flags;
                info.activate = activate;
                info.propagate = propagate;

                if (it->keyListenerInfo.end() == it->keyListenerInfo.find(keyCode))
                {
                  it->keyListenerInfo[keyCode] = std::vector<KeyListenerInfo>();
                  it->keyListenerInfo[keyCode].push_back(info);
                }
                else
                {
                  std::vector<KeyListenerInfo>& keyListenerEntry = it->keyListenerInfo[keyCode];
                  bool isEntryAvailable = false;
                  for (int i=0; i<keyListenerEntry.size(); i++) {
                    struct KeyListenerInfo& listenerInfo = keyListenerEntry[i];
                    if (listenerInfo.flags == flags)
                    {
                      listenerInfo.activate = activate;
                      listenerInfo.propagate = propagate;
                      isEntryAvailable = true;
                      break;
                    }
                  }
                  if (false == isEntryAvailable) {
                    keyListenerEntry.push_back(info);
                  }
                }
                return true;
            }
        }
        return false;
    }

    bool CompositorController::addNativeKeyListener(const std::string& client, const uint32_t& keyCode, const uint32_t& flags, std::map<std::string, RdkShellData> &listenerProperties)
    {
        uint32_t mappedKeyCode = 0, mappedFlags = 0;
        keyCodeFromWayland(keyCode, flags, mappedKeyCode, mappedFlags);

        std::cout << "Native keyCode: " << keyCode << " flags: " << flags
                  << " converted to RDKShell keyCode: " << mappedKeyCode << " flags: " << mappedFlags << std::endl;

        return CompositorController::addKeyListener(client, mappedKeyCode, mappedFlags, listenerProperties);
    }

    bool CompositorController::removeKeyListener(const std::string& client, const uint32_t& keyCode, const uint32_t& flags)
    {
        std::string clientDisplayName = standardizeName(client);

        std::cout << "key listener removed client: " << client.c_str() << " RDKShell keyCode " << keyCode << " flags " << flags << std::endl;

        for (std::vector<CompositorInfo>::iterator it = gCompositorList.begin(); it != gCompositorList.end(); ++it)
        {
            if (it->name == clientDisplayName)
            {
                if (it->keyListenerInfo.end() != it->keyListenerInfo.find(keyCode))
                {
                  bool isEntryAvailable = false;
                  std::vector<KeyListenerInfo>::iterator entryPos = it->keyListenerInfo[keyCode].end();
                  for (std::vector<KeyListenerInfo>::iterator iter = it->keyListenerInfo[keyCode].begin() ; iter != it->keyListenerInfo[keyCode].end(); ++iter)
                  {
                    if ((*iter).flags == flags)
                    {
                      entryPos = iter;
                      isEntryAvailable = true;
                      break;
                    }
                  }
                  if (true == isEntryAvailable) {
                    if ((it->compositor) && (it->compositor->isKeyPressed()))
                    {
                        gPendingKeyUpListeners.push_back(it->compositor);
                    }
                    it->keyListenerInfo[keyCode].erase(entryPos);
                    if (it->keyListenerInfo[keyCode].size() == 0) {
                      it->keyListenerInfo.erase(keyCode);
                    }
                  }
                }
                return true;
            }
        }
        return false;
    }

    bool CompositorController::removeNativeKeyListener(const std::string& client, const uint32_t& keyCode, const uint32_t& flags)
    {
        uint32_t mappedKeyCode = 0, mappedFlags = 0;
        keyCodeFromWayland(keyCode, flags, mappedKeyCode, mappedFlags);

        std::cout << "Native keyCode: " << keyCode << " flags: " << flags
                  << " converted to RDKShell keyCode: " << mappedKeyCode << " flags: " << mappedFlags << std::endl;

        return CompositorController::removeKeyListener(client, mappedKeyCode, mappedFlags);
    }

    bool CompositorController::addKeyMetadataListener(const std::string& client)
    {
        std::string clientDisplayName = standardizeName(client);
        for (auto compositor : gCompositorList)
        {
            if (compositor.name == clientDisplayName && compositor.compositor != nullptr)
            {
                compositor.compositor->setKeyMetadataEnabled(true);
                return true;
            }
        }
        return false;
    }

    bool CompositorController::removeKeyMetadataListener(const std::string& client)
    {
        std::string clientDisplayName = standardizeName(client);
        for (auto compositor : gCompositorList)
        {
            if (compositor.name == clientDisplayName && compositor.compositor != nullptr)
            {
                compositor.compositor->setKeyMetadataEnabled(false);
                return true;
            }
        }
        return false;
    }

    bool CompositorController::injectKey(const uint32_t& keyCode, const uint32_t& flags)
    {
        CompositorController::onKeyPress(keyCode, flags, 0);
        CompositorController::onKeyRelease(keyCode, flags, 0);
        return true;
    }

    bool CompositorController::generateKey(const std::string& client, const uint32_t& keyCode, const uint32_t& flags)
    {
        bool ret = false;
        if (client.empty())
        {
            CompositorController::onKeyPress(keyCode, flags, 0);
            CompositorController::onKeyRelease(keyCode, flags, 0);
            ret = true;
        }
        else
        {
            std::string clientDisplayName = standardizeName(client);
            for (auto compositor : gCompositorList)
            {
                if (compositor.name == clientDisplayName && compositor.compositor != nullptr)
                {
                    compositor.compositor->onKeyPress(keyCode, flags, 0);
                    compositor.compositor->onKeyRelease(keyCode, flags, 0);
                    ret = true;
                    break;
                }
            }
        }
        return ret;
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
                if (o <= 0.0)
                {
                    o = 0.0;
                }
                opacity = (unsigned int)(o * 100);
                if (opacity > 100)
                {
                     opacity = 100;
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

    bool CompositorController::getHolePunch(const std::string& client, bool& holePunch)
    {
        std::string clientDisplayName = standardizeName(client);
        for (auto compositor : gCompositorList)
        {
            if (compositor.name == clientDisplayName)
            {
                compositor.compositor->holePunch(holePunch);
                return true;
            }
        }
        return false;
    }

    bool CompositorController::setHolePunch(const std::string& client, const bool holePunch)
    {
        std::string clientDisplayName = standardizeName(client);
        for (auto compositor : gCompositorList)
        {
            if (compositor.name == clientDisplayName)
            {
                compositor.compositor->setHolePunch(holePunch);
                RdkShell::Logger::log(RdkShell::LogLevel::Information, "hole punch for %s set to %s", clientDisplayName.c_str(), holePunch ? "true" : "false");
                return true;
            }
        }
        return false;
    }

    bool CompositorController::scaleToFit(const std::string& client, const int32_t x, const int32_t y, const uint32_t width, const uint32_t height)
    {
        std::string clientDisplayName = standardizeName(client);
        for (auto compositor : gCompositorList)
        {
            if (compositor.name == clientDisplayName && compositor.compositor != nullptr)
            {
                uint32_t currentWidth = 0;
                uint32_t currentHeight = 0;
                compositor.compositor->size(currentWidth, currentHeight);

                double scaleX = (double)width / (double)currentWidth;
                double scaleY = (double)height / (double)currentHeight;

                compositor.compositor->setPosition(x, y);
                compositor.compositor->setScale(scaleX, scaleY);
                return true;
            }
        }
        return true;
    }

    void CompositorController::onKeyPress(uint32_t keycode, uint32_t flags, uint64_t metadata)
    {
        //std::cout << "key press code " << keycode << " flags " << flags << std::endl;
        double currentTime = RdkShell::seconds();
        if (0.0 == gLastKeyPressStartTime)
        {
            gLastKeyPressStartTime = currentTime;
        }
        gLastKeyEventTime = currentTime;
        gNextInactiveEventTime = gLastKeyEventTime + gInactivityIntervalInSeconds;

        bool isInterceptAvailable = false;

        isInterceptAvailable = interceptKey(keycode, flags, metadata, true);

        if (false == isInterceptAvailable && gFocusedCompositor.compositor)
        {
            gFocusedCompositor.compositor->onKeyPress(keycode, flags, metadata);
            bubbleKey(keycode, flags, metadata, true);
        }
        else
        {
            std::string focusedClientName = !gFocusedCompositor.name.empty() ? gFocusedCompositor.name : "none";
            std::cout << "rdkshell_focus key intercepted: " << isInterceptAvailable << " focused client:" << focusedClientName << std::endl;
        }

    }

    void CompositorController::onKeyRelease(uint32_t keycode, uint32_t flags, uint64_t metadata)
    {
        //std::cout << "key release code " << keycode << " flags " << flags << std::endl;
        double keyPressTime = RdkShell::seconds() - gLastKeyPressStartTime;
        checkEasterEggs(keycode, flags, keyPressTime);
        gLastKeyEventTime = RdkShell::seconds();
        gLastKeyPressStartTime = 0.0;
        gNextInactiveEventTime = gLastKeyEventTime + gInactivityIntervalInSeconds;

        bool isInterceptAvailable = false;
        isInterceptAvailable = interceptKey(keycode, flags, metadata, false);

        if (false == isInterceptAvailable)
        {
            if (gFocusedCompositor.compositor)
            {
                gFocusedCompositor.compositor->onKeyRelease(keycode, flags, metadata);
                bubbleKey(keycode, flags, metadata, false);
            }
        }
        for ( const auto &compositor : gPendingKeyUpListeners )
        {
            compositor->onKeyRelease(keycode, flags, metadata);
        }
        gPendingKeyUpListeners.clear();

        if (keycode != 0 && gPowerKeyEnabled && keycode == gPowerKeyCode)
        {
            RdkShell::Logger::log(RdkShell::LogLevel::Information, "power key pressed");
            if (gRdkShellEventListener)
            {
                RdkShell::Logger::log(RdkShell::LogLevel::Information, "sending the power key event");
                gRdkShellEventListener->onPowerKey();
            }
        }
    }

    bool CompositorController::createDisplay(const std::string& client, const std::string& displayName, uint32_t displayWidth, uint32_t displayHeight)
    {
        std::cout << "rdkshell createDisplay client: " << client << " displayName: " << displayName << "\n";
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
        if (gRdkShellCompositorType == SURFACE)
        {
            compositorInfo.compositor = std::make_shared<RdkCompositorSurface>();
        }
        else
        {
            compositorInfo.compositor = std::make_shared<RdkCompositorNested>();
        }
        uint32_t width = 0;
        uint32_t height = 0;
        RdkShell::EssosInstance::instance()->resolution(width, height);
        if (displayWidth > 0)
        {
            width = displayWidth;
        }
        if (displayHeight > 0)
        {
            height = displayHeight;
        }
        bool ret = compositorInfo.compositor->createDisplay(compositorDisplayName, clientDisplayName, width, height);
        if (ret)
        {
          if (gCompositorList.empty())
          {
              gFocusedCompositor = compositorInfo;
              std::cout << "rdkshell_focus create: setting focus of first application created " << gFocusedCompositor.name << std::endl;
          }
          //std::cout << "display created with name: " << client << std::endl;
          gCompositorList.insert(gCompositorList.begin(), compositorInfo);
        }
        return ret;
    }

    bool CompositorController::draw()
    {
        if (gShowWaterMarkImage && gWaterMarkImage != nullptr)
        {
            gWaterMarkImage->draw();
        }

        for (std::vector<CompositorInfo>::reverse_iterator reverseIterator = gCompositorList.rbegin() ; reverseIterator != gCompositorList.rend(); reverseIterator++)
        {
            reverseIterator->compositor->draw();
        }

        if (gShowSplashImage && gSplashImage != nullptr)
        {
            if (gSplashDisplayTimeInSeconds > 0)
            {
                uint32_t splashShownTime = (uint32_t)(RdkShell::seconds() - gSplashStartTime);
                if (splashShownTime > gSplashDisplayTimeInSeconds)
                {
                    RdkShell::Logger::log(RdkShell::LogLevel::Information, "hiding the splash screen after a timeout: %u", gSplashDisplayTimeInSeconds );
                    hideSplashScreen();
                }
                else
                {
                    gSplashImage->draw();
                }
            }
            else
            {
                gSplashImage->draw();
            }
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
                double opacity = 1.0;
                double delay = 0.0;
                std::string tween = "linear";
                if (compositor.compositor != nullptr)
                {
                    //retrieve the initial values in case they are not specified in the property set
                    compositor.compositor->position(x, y);
                    compositor.compositor->size(width, height);
                    compositor.compositor->scale(scaleX, scaleY);
                    compositor.compositor->opacity(opacity);
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
                    else if (property.first == "a")
                    {
                        double opacityPercent = property.second.toDouble();
                        opacity = opacityPercent / 100.0;
                    }
                    else if (property.first == "tween")
                    {
                        tween = property.second.toString();
                    }
                    else if (property.first == "delay")
                    {
                        delay = property.second.toDouble();
                    }
                }

                animation.compositor = compositor.compositor;
                animation.endX = x;
                animation.endY = y;
                animation.endWidth = width;
                animation.endHeight = height;
                animation.endScaleX = scaleX;
                animation.endScaleY = scaleY;
                animation.endOpacity = opacity;
                animation.duration = duration;
                animation.name = client;
                animation.tween = tween;
                animation.delay = delay;
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
        resolveWaitingEasterEggs();
        RdkShell::Animator::instance()->animate();
        if (gEnableInactivityReporting)
        {
            double currentTime = RdkShell::seconds();
            if (currentTime > gNextInactiveEventTime)
            {
                if (gRdkShellEventListener)
                {
                    gRdkShellEventListener->onUserInactive(getInactivityTimeInMinutes());
                }
              gNextInactiveEventTime = currentTime + gInactivityIntervalInSeconds;
            }
        }
        return true;
    }

    bool CompositorController::addListener(const std::string& client, std::shared_ptr<RdkShellEventListener> listener)
    {
        std::string clientDisplayName = standardizeName(client);
        for (auto it = gCompositorList.begin(); it != gCompositorList.end(); ++it)
        {
           if (it->name == clientDisplayName)
           {
              it->eventListeners.push_back(listener);
              break;
           }
        }
        return true;
    }

    bool CompositorController::removeListener(const std::string& client, std::shared_ptr<RdkShellEventListener> listener)
    {
        std::string clientDisplayName = standardizeName(client);
        for (auto it = gCompositorList.begin(); it != gCompositorList.end(); ++it)
        {
            if (it->name == clientDisplayName)
            {
                std::vector<std::shared_ptr<RdkShellEventListener>>::iterator entryToRemove = it->eventListeners.end();
                for (std::vector<std::shared_ptr<RdkShellEventListener>>::iterator iter = it->eventListeners.begin() ; iter != it->eventListeners.end(); ++iter)
                {
                  if ((*iter) == listener)
                  {
                    entryToRemove = iter;
                    break;
                  }
                }
                if (entryToRemove != it->eventListeners.end())
                {
                  it->eventListeners.erase(entryToRemove);
                }
                break;
            }
        }
        return true;
    }

    bool CompositorController::onEvent(RdkCompositor* eventCompositor, const std::string& eventName)
    {
        for (auto compositor : gCompositorList)
         {
            if (compositor.compositor.get() == eventCompositor)
            {
                for (int i=0; i<compositor.eventListeners.size(); i++)
                {
                    sendApplicationEvent(compositor.eventListeners[i], eventName, compositor.name);
                }
                break;
            }
        }
        return true;
    }

    void CompositorController::setEventListener(std::shared_ptr<RdkShellEventListener> listener)
    {
        gRdkShellEventListener = listener;
    }

    void CompositorController::enableInactivityReporting(bool enable)
    {
        gEnableInactivityReporting = enable;
    }

    void CompositorController::setInactivityInterval(double minutes)
    {
        gInactivityIntervalInSeconds = minutes * 60;
        gNextInactiveEventTime = gLastKeyEventTime + gInactivityIntervalInSeconds;
    }

    double CompositorController::getInactivityTimeInMinutes()
    {
        double inactiveTimeInSeconds = RdkShell::seconds() - gLastKeyEventTime;
        return (inactiveTimeInSeconds / 60.0);
    }

    bool CompositorController::launchApplication(const std::string& client, const std::string& uri, const std::string& mimeType)
    {
        if (mimeType == RDKSHELL_APPLICATION_MIME_TYPE_NATIVE)
        {
            std::string clientDisplayName = standardizeName(client);
            for (auto compositor : gCompositorList)
            {
                if (compositor.name == clientDisplayName)
                {
                    std::cout << "application with name " << client << " is already launched\n";
                    return false;
                }
            }
            CompositorInfo compositorInfo;
            compositorInfo.name = clientDisplayName;
            if (gRdkShellCompositorType == SURFACE)
            {
                compositorInfo.compositor = std::make_shared<RdkCompositorSurface>();
            }
            else
            {
                compositorInfo.compositor = std::make_shared<RdkCompositorNested>();
            }
            compositorInfo.mimeType = RDKSHELL_APPLICATION_MIME_TYPE_NATIVE;
            uint32_t width = 0;
            uint32_t height = 0;
            RdkShell::EssosInstance::instance()->resolution(width, height);
            compositorInfo.compositor->setApplication(uri);
            bool ret = compositorInfo.compositor->createDisplay(clientDisplayName, "", width, height);
            if (ret)
            {
                if (gCompositorList.empty())
                {
                    gFocusedCompositor = compositorInfo;
                    std::cout << "rdkshell_focus launch: setting focus of first application created " << gFocusedCompositor.name << std::endl;
                }
                gCompositorList.insert(gCompositorList.begin(), compositorInfo);
            }
            return true;
        }
        else
        {
            std::cout << "unable to launch application.  mime type " << mimeType << " is not supported\n";
        }

        return false;
    }

    bool CompositorController::suspendApplication(const std::string& client)
    {
        std::string clientDisplayName = standardizeName(client);
        for (auto compositor : gCompositorList)
        {
            if (compositor.name == clientDisplayName)
            {
                if (compositor.compositor != nullptr)
                {
                    bool result = compositor.compositor->suspendApplication();
                    if (result)
                    {
                        std::cout << client << " has been suspended\n";
                        compositor.compositor->setVisible(false);
                    }
                    return result;
                }
                std::cout << "display with name " << client << " did not have a valid compositor\n";
                return false;
            }
        }
        std::cout << "display with name " << client << " was not found\n";
        return false;
    }

    bool CompositorController::resumeApplication(const std::string& client)
    {
        std::string clientDisplayName = standardizeName(client);
        for (auto compositor : gCompositorList)
        {
            if (compositor.name == clientDisplayName)
            {
                if (compositor.compositor != nullptr)
                {
                    bool result = compositor.compositor->resumeApplication();
                    if (result)
                    {
                        std::cout << client << " has been resumed \n";
                        compositor.compositor->setVisible(true);
                    }
                }
                std::cout << "display with name " << client << " did not have a valid compositor\n";
                return false;
            }
        }
        std::cout << "display with name " << client << " was not found\n";
        return false;
    }

    bool CompositorController::closeApplication(const std::string& client)
    {
        std::string clientDisplayName = standardizeName(client);
        for (auto compositor : gCompositorList)
        {
            if (compositor.name == clientDisplayName)
            {
                if (compositor.compositor != nullptr)
                {
                    compositor.compositor->closeApplication();
                    return true;
                }
                std::cout << "display with name " << client << " did not have a valid compositor\n";
                return false;
            }
        }
        std::cout << "display with name " << client << " was not found\n";
        return false;
    }

    bool CompositorController::getMimeType(const std::string& client, std::string& mimeType)
    {
        mimeType = "";
        std::string clientDisplayName = standardizeName(client);
        for (const auto& compositor : gCompositorList)
        {
            if (compositor.name == clientDisplayName)
            {
                mimeType = compositor.mimeType;
                return true;
            }
        }
        return false;
    }

    bool CompositorController::setMimeType(const std::string& client, const std::string& mimeType)
    {
        std::string clientDisplayName = standardizeName(client);
        for (auto&& compositor : gCompositorList)
        {
            if (compositor.name == clientDisplayName)
            {
                compositor.mimeType = mimeType;
                return true;
            }
        }
        return false;
    }

    bool CompositorController::hideWatermark()
    {
        gShowWaterMarkImage = false;
        gWaterMarkImage = nullptr;
        return true;
    }

    bool CompositorController::showWatermark()
    {
        RdkShell::Logger::log(RdkShell::LogLevel::Information, "attempting to display watermark");
        if (nullptr == gWaterMarkImage)
        {
            const char* waterMarkFile = getenv("RDKSHELL_WATERMARK_IMAGE_PNG");
            if (waterMarkFile)
            {
                gWaterMarkImage = std::make_shared<RdkShell::Image>();
                bool imageLoaded = gWaterMarkImage->loadLocalFile(waterMarkFile);
                if (!imageLoaded)
                {
                    RdkShell::Logger::log(RdkShell::LogLevel::Error, "error loading watermark image: %s", waterMarkFile);
                    gWaterMarkImage = nullptr;
                    return false;
                }
            }
            else
            {
                RdkShell::Logger::log(RdkShell::LogLevel::Warn, "no watermark image specified");
                return false;
            }
        }
        gShowWaterMarkImage = true;
        return true;
    }

    bool CompositorController::hideSplashScreen()
    {
        gShowSplashImage = false;
        gSplashImage = nullptr;
        return true;
    }

    bool CompositorController::showSplashScreen(uint32_t displayTimeInSeconds)
    {
        RdkShell::Logger::log(RdkShell::LogLevel::Information, "attempting to display splash image with time: %u", displayTimeInSeconds);
        if (!gShowSplashImage)
        {
            const char* splashFile = getenv("RDKSHELL_SPLASH_IMAGE_JPEG");
            if (splashFile)
            {
                gSplashImage = std::make_shared<RdkShell::Image>();
                gShowSplashImage = gSplashImage->loadLocalFile(splashFile);
                if (!gShowSplashImage)
                {
                    RdkShell::Logger::log(RdkShell::LogLevel::Error, "error loading splash image: %s", splashFile);
                    gSplashImage = nullptr;
                    return false;
                }
            }
            else
            {
                RdkShell::Logger::log(RdkShell::LogLevel::Warn, "no splash image specified");
                return false;
            }
            gSplashDisplayTimeInSeconds = displayTimeInSeconds;
            gSplashStartTime = RdkShell::seconds();
        }
        return true;
    }

    bool CompositorController::setLogLevel(const std::string level)
    {
        Logger::setLogLevel(level.c_str());
        return true;
    }

    bool CompositorController::getLogLevel(std::string& level)
    {
        Logger::logLevel(level);
        return true;
    }

    bool CompositorController::sendEvent(const std::string& eventName, std::vector<std::map<std::string, RdkShellData>>& data)
    {
        if (eventName.compare(RDKSHELL_EVENT_DEVICE_LOW_RAM_WARNING) == 0)
        {
            int32_t freeKb = -1;
            if (!data.empty())
            {
                freeKb = data[0]["freeKb"].toInteger32();
            }
            gRdkShellEventListener->onDeviceLowRamWarning(freeKb);
        }
        else if (eventName.compare(RDKSHELL_EVENT_DEVICE_CRITICALLY_LOW_RAM_WARNING) == 0)
        {
            int32_t freeKb = -1;
            if (!data.empty())
            {
                freeKb = data[0]["freeKb"].toInteger32();
            }
            gRdkShellEventListener->onDeviceCriticallyLowRamWarning(freeKb);
        }
        else if (eventName.compare(RDKSHELL_EVENT_DEVICE_LOW_RAM_WARNING_CLEARED) == 0)
        {
            int32_t freeKb = -1;
            if (!data.empty())
            {
                freeKb = data[0]["freeKb"].toInteger32();
            }
            gRdkShellEventListener->onDeviceLowRamWarningCleared(freeKb);
        }
        else if (eventName.compare(RDKSHELL_EVENT_DEVICE_CRITICALLY_LOW_RAM_WARNING_CLEARED) == 0)
        {
            int32_t freeKb = -1;
            if (!data.empty())
            {
                freeKb = data[0]["freeKb"].toInteger32();
            }
            gRdkShellEventListener->onDeviceCriticallyLowRamWarningCleared(freeKb);
        }
        else if (eventName.compare(RDKSHELL_EVENT_ANIMATION) == 0)
        {
            if (!data.empty())
            {
              gRdkShellEventListener->onAnimation(data);
            }
        }
        else if (eventName.compare(RDKSHELL_EVENT_EASTER_EGG) == 0)
        {
            std::string name(""), actionJson("");
            if (!data.empty())
            {
                name = data[0]["name"].toString();
                actionJson = data[0]["action"].toString();
            }
            gRdkShellEventListener->onEasterEgg(name, actionJson);
        }
        return true;
    }
}
